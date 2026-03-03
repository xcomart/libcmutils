/*
MIT License

Copyright (c) 2020 Dennis Soungjin Park<xcomart@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include <time.h>

#include "functions.h"
#if defined(APPLE)
#include <sys/syslimits.h>
#elif defined(MSWIN)
#define PATH_MAX MAX_PATH
#elif defined(LINUX)
#include <limits.h>
#endif

CMUTIL_LogDefine("cmutils.http")

struct CMUTIL_HttpContext {
    CMUTIL_Map      *socket_pools;
    CMUTIL_Mutex    *socket_pools_mutex;
    CMUTIL_Timer    *timer;
};

typedef struct CMUTIL_SocketPoolElem {
    time_t          last_used;
    CMUTIL_Mem      *memst;
    char            host[256];
    int             port;
    CMUTIL_Socket   *sock;
} CMUTIL_SocketPoolElem;

static struct CMUTIL_HttpContext g_httpctx;

void CMUTIL_HttpInit()
{
    memset(&g_httpctx, 0, sizeof(g_httpctx));
    g_httpctx.socket_pools = CMUTIL_MapCreateEx(
                10, CMFalse, NULL, 0.75f);
    g_httpctx.socket_pools_mutex = CMUTIL_MutexCreate();
    g_httpctx.timer = CMUTIL_TimerCreateEx(1000, 1);
}

void CMUTIL_HttpClear()
{
    if (g_httpctx.socket_pools) {
        CMUTIL_StringArray *keys = CMCall(g_httpctx.socket_pools, GetKeys);
        int i;
        for (i=0; i<CMCall(keys, GetSize); i++) {
            const CMUTIL_String *key = CMCall(keys, GetAt, i);
            const char* skey = CMCall(key, GetCString);
            CMUTIL_Pool *pool = CMCall(g_httpctx.socket_pools, Get, skey);
            CMCall(pool, Destroy);
        }
        CMCall(keys, Destroy);
        CMCall(g_httpctx.socket_pools, Destroy);
    }
    if (g_httpctx.socket_pools_mutex)
        CMCall(g_httpctx.socket_pools_mutex, Destroy);
    if (g_httpctx.timer)
        CMCall(g_httpctx.timer, Destroy);
    memset(&g_httpctx, 0, sizeof(g_httpctx));
}

CMUTIL_STATIC CMUTIL_SocketPoolElem *CMUTIL_SocketPoolElemCreate(
    CMUTIL_Mem *memst, CMUTIL_Socket *sock, const char *host, int port)
{
    CMUTIL_SocketPoolElem *res = memst->Alloc(sizeof(CMUTIL_SocketPoolElem));
    memset(res, 0x0, sizeof(CMUTIL_SocketPoolElem));
    res->sock = sock;
    res->memst = memst;
    res->last_used = time(NULL);
    strncpy(res->host, host, sizeof(res->host) - 1);
    res->host[sizeof(res->host) - 1] = '\0';
    res->port = port;
    return res;
}

CMUTIL_STATIC void CMUTIL_SocketPoolDestroy(
    void *resource, void *udata)
{
    CMUTIL_SocketPoolElem *elem = resource;
    if (elem) {
        CMUTIL_Socket *sock = elem->sock;
        if (sock) {
            if (CMLogIsEnabled(CMLogLevel_Trace)) {
                CMLogTrace("close socket(%s:%d)", elem->host, elem->port);
            }
            CMCall(sock, Close);
        }
        elem->memst->Free(elem);
    }
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_HttpContextGetSocket(
    const char *host, int port)
{
    CMUTIL_Pool *pool = NULL;
    CMUTIL_SocketPoolElem *elem = NULL;
    char buf[256];
    sprintf(buf, "%s:%d", host, port);
    CMCall(g_httpctx.socket_pools_mutex, Lock);
    pool = CMCall(g_httpctx.socket_pools, Get, buf);
    if (pool)
        elem = CMCall(pool, CheckOut, 0L);
    CMCall(g_httpctx.socket_pools_mutex, Unlock);
    if (elem) {
        CMUTIL_Socket *sock = elem->sock;
        elem->sock = NULL;
        CMUTIL_SocketPoolDestroy(elem, NULL);
        CMLogTrace("get socket(%s:%d) from pool", host, port);
        return sock;
    }
    return NULL;
}

CMUTIL_STATIC CMBool CMUTIL_SocketPoolTest(
    void *resource, void *udata)
{
    CMUTIL_SocketPoolElem *elem = resource;
    if (elem) {
        CMBool res = CMTrue;
        const CMUTIL_Socket *sock = elem->sock;
        // destroy after 30 seconds
        if (time(NULL) - elem->last_used > 30) {
            res = CMFalse;
        }
        if (res && sock) {
            if (CMCall(sock, CheckReadBuffer, 0L) != CMSocketTimeout) {
                // server will not send data before request.
                // so this connection may be closed
                res = CMFalse;
            }
        }
        return res;
    }
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_HttpContextPutSocket(
    const char *host, int port, CMUTIL_Socket *sock)
{
    CMUTIL_Pool *pool = NULL;
    char buf[256];
    CMBool res = CMFalse;
    sprintf(buf, "%s:%d", host, port);
    CMCall(g_httpctx.socket_pools_mutex, Lock);
    pool = CMCall(g_httpctx.socket_pools, Get, buf);
    if (pool == NULL) {
        pool = CMUTIL_PoolCreate(0, 5, NULL,
            CMUTIL_SocketPoolDestroy,
            CMUTIL_SocketPoolTest,
            1000L,
            CMFalse,
            NULL,
            g_httpctx.timer);
        CMCall(g_httpctx.socket_pools, Put, buf, pool, NULL);
    }
    if (pool) {
        CMCall(pool, Release,
            CMUTIL_SocketPoolElemCreate(CMUTIL_GetMem(), sock, host, port));
        CMLogTrace("put socket(%s:%d) to pool", host, port);
        res = CMTrue;
    }
    CMCall(g_httpctx.socket_pools_mutex, Unlock);
    return res;
}

typedef struct CMUTIL_HttpClient_Internal {
    CMUTIL_HttpClient   base;
    CMUTIL_Mem          *memst;
    char                host[256];
    int                 port;
    CMBool              ishttps;
    CMBool              verifypeer;
    CMBool              verifyhost;
    CMBool              keepalive;
    char                cafile[PATH_MAX];
    char                keyfile[PATH_MAX];
    char                certfile[PATH_MAX];
} CMUTIL_HttpClient_Internal;

CMUTIL_STATIC CMBool CMUTIL_HttpClientSetVerify(
    CMUTIL_HttpClient *client, CMBool verify_host, CMBool verify_peer)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;
    ih->verifyhost = verify_host;
    ih->verifypeer = verify_peer;
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_HttpClientSetSSLCert(
    CMUTIL_HttpClient *client,
    const char *certfile,
    const char *keyfile,
    const char *cafile)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;
    strcpy(ih->certfile, certfile);
    strcpy(ih->keyfile, keyfile);
    strcpy(ih->cafile, cafile);
    return CMTrue;
}

CMUTIL_STATIC void CMUTIL_HttpClientSetKeepAlive(
    CMUTIL_HttpClient *client,
    CMBool keepalive)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;
    ih->keepalive = keepalive;
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_HttpClientGetSocket(
    CMUTIL_HttpClient *client, long timeout)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;

    CMUTIL_Socket *sock = CMUTIL_HttpContextGetSocket(ih->host, ih->port);
    if (sock == NULL) {
        // create a new socket
        if (ih->ishttps) {
            sock = CMUTIL_SSLSocketConnectInternal(
                ih->memst,
                ih->verifypeer ? ih->certfile : NULL,
                ih->verifypeer ? ih->keyfile : NULL,
                ih->verifyhost ? ih->cafile : NULL,
                ih->verifyhost ? ih->host : NULL,
                ih->host,
                ih->port,
                timeout,
                CMFalse);
        } else {
            sock = CMUTIL_SocketConnectInternal(
                ih->memst,
                ih->host,
                ih->port,
                timeout,
                CMFalse,
                CMFalse);
        }
        if (sock == NULL) {
            CMLogError("failed to connect to %s:%d", ih->host, ih->port);
            return NULL;
        }
    }

    return sock;
}

CMUTIL_STATIC CMBool CMUTIL_HttpClientReadLine(
    CMUTIL_Socket *sock, char *buf, size_t len, long timeout)
{
    size_t i = 0;
    while (i < len - 1) {
        int b = CMCall(sock, ReadByte, timeout);
        if (b > -1) {
            if (b == '\n') {
                buf[i] = '\0';
                CMLogTrace("Read -> %s", buf);
                return CMTrue;
            }
            if (b != '\r')
                buf[i++] = (char)b;
        } else {
            buf[i] = '\0';
            CMLogError("Read timeout: [%s]", buf);
            return CMFalse;
        }
    }
    buf[i] = '\0';
    CMLogError("Buffer too small: %s", buf);
    return CMFalse;
}

CMUTIL_STATIC char *CMUTIL_HttpClientLineValue(char *buf, const char *header)
{
    char hbuf[128];
    char *p = strchr(buf, ':');
    char *q = p;
    if (p == NULL) return NULL;
    strncpy(hbuf, buf, p-buf);
    hbuf[p-buf] = '\0';
    p = CMUTIL_StrTrim(hbuf);
    if (strcasecmp(p, header) == 0) {
        q++;
        return CMUTIL_StrTrim(q);
    }
    return NULL;
}

CMUTIL_STATIC CMSocketResult CMUTIL_HttpClientWriteLine(
    CMUTIL_Socket *sock, const char *line, long timeout)
{
    CMSocketResult sr = CMSocketOk;
    if (line && *line)
        sr = CMCall(sock, Write, line, strlen(line), timeout);
    if (sr == CMSocketOk)
        sr = CMCall(sock, Write, "\r\n", 2, timeout);
    CMLogTrace("Write -> %s", line);
    return sr;
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_HttpClientRequest(
    CMUTIL_HttpClient *client,
    const char *method,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_ByteBuffer *body,
    int *status,
    long timeout)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;
    CMUTIL_ByteBuffer *res = NULL;
    CMSocketResult sr;
    const CMUTIL_Array *pairs = NULL;
    char buf[4096];
    char encoding[1024] = "";
    char transfer[1024] = "";
    size_t len = 0;
    CMBool keepalive = ih->keepalive;
    char *p;
    CMBool needHost = CMTrue;
    CMBool needLength = CMTrue;

    CMUTIL_Socket *sock = CMUTIL_HttpClientGetSocket(client, timeout);
    if (sock == NULL) {
        CMLogError("failed to get socket");
        return NULL;
    }

    sprintf(buf, "%s %s HTTP/1.1", method, uri);
    sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
    if (sr != CMSocketOk) {
        CMLogError("failed to write request line");
        goto FAILED;
    }

    if (headers)
        pairs = CMCall(headers, GetPairs);
    if (pairs) {
        int i;
        for (i=0; i<CMCall(pairs, GetSize); i++) {
            CMUTIL_MapPair *pair = CMCall(pairs, GetAt, i);
            sprintf(buf, "%s: %s",
                CMCall(pair, GetKey),
                (char*)CMCall(pair, GetValue));
            if (strcasecmp("Host", CMCall(pair, GetKey)) == 0)
                needHost = CMFalse;
            if (strcasecmp("Content-Length", CMCall(pair, GetKey)) == 0)
                needLength = CMFalse;
            sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
            if (sr != CMSocketOk) {
                CMLogError("failed to write header line");
                goto FAILED;
            }
        }
    }
    if (needHost) {
        sprintf(buf, "Host: %s:%d", ih->host, ih->port);
        sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
        if (sr != CMSocketOk) {
            CMLogError("failed to write header line");
            goto FAILED;
        }
    }
    if (!ih->keepalive) {
        sprintf(buf, "Connection: close");
    } else {
        sprintf(buf, "Connection: keep-alive");
    }
    sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
    if (sr != CMSocketOk) {
        CMLogError("failed to write header line");
        goto FAILED;
    }
    if (strcasecmp(method, "POST") == 0 || strcasecmp(method, "PUT") == 0) {
        if (body != NULL) {
            if (needLength) {
                sprintf(buf, "Content-Length: %zu", CMCall(body, GetSize));
                sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
                if (sr != CMSocketOk) {
                    CMLogError("failed to write header line");
                    goto FAILED;
                }
            }
        } else {
            CMLogWarn("request body is empty for method %s", method);
        }
    }

    sr = CMUTIL_HttpClientWriteLine(sock, NULL, timeout);
    if (sr != CMSocketOk) {
        CMLogError("failed to write header end");
        goto FAILED;
    }

    if (strcasecmp(method, "POST") == 0 || strcasecmp(method, "PUT") == 0) {
        if (body != NULL) {
            const uint8_t *data = CMCall(body, GetBytes);
            const size_t size = CMCall(body, GetSize);
            sr = CMCall(sock, Write, data, size, timeout);
            // write last crlf
            if (sr == CMSocketOk)
                sr = CMUTIL_HttpClientWriteLine(sock, NULL, timeout);
            if (sr != CMSocketOk) {
                CMLogError("failed to write request body");
                goto FAILED;
            }
        } else {
            CMLogWarn("request body is empty for method %s", method);
        }
    }

    if (!CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout)) {
        CMLogError("failed to read response line");
        goto FAILED;
    }

    {
        char *q;
        CMLogTrace("status line: %s", buf);
        p = strchr(buf, ' ');
        if (!p) {
            CMLogError("failed to parse status line");
            goto FAILED;
        }
        q = strchr(p, ' ');
        if (q) *q = '\0';
        *status = (int)strtol(p+1, NULL, 10);
        if (*status != 200) {
            CMLogWarn("status code is not 200: %d - %s", *status, q+1);
       }
    }

    // read headers
    while (CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout)) {
        if (strlen(buf) == 0) {
            break;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Connection");
        if (p) {
            if (strcmp(p, "close") == 0) {
                keepalive = CMFalse;
            }
            continue;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Content-Length");
        if (p) {
            len = strtol(p, NULL, 10);
            continue;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Transfer-Encoding");
        if (p) {
            strncpy(transfer, p, sizeof(transfer) - 1);
            continue;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Content-Encoding");
        if (p) {
            strncpy(encoding, p, sizeof(encoding) - 1);
            continue;
        }
    }
    if (strlen(buf) != 0) {
        CMLogError("failed to parse headers: %s", buf);
        goto FAILED;
    }

    if (len > 0) {
        res = CMUTIL_ByteBufferCreateInternal(ih->memst, len);
        sr = CMCall(sock, Read, res, len, timeout);
        if (sr == CMSocketOk) {
            goto ENDPOINT;
        }
        CMLogError("failed to read response body");
    } else {
        if (strstr(transfer, "chunked")) {
            res = CMUTIL_ByteBufferCreateInternal(ih->memst, 2048);
            while (CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout)) {
                if (*buf == '\0')
                    break;
                len = strtol(buf, NULL, 16);
                if (len == 0) {
                    // consume chunk end \r\n
                    CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout);
                    break;
                }
                sr = CMCall(sock, Read, res, len, timeout);
                if (sr != CMSocketOk) {
                    CMLogError("failed to read response body");
                    goto FAILED;
                }
                // consume chunk end \r\n
                CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout);
            }
        } else {
            CMLogError("unknown transfer encoding: %s", transfer);
            goto FAILED;
       }
    }
    if (keepalive) {
        // save connection for later use.
        CMUTIL_HttpContextPutSocket(ih->host, ih->port, sock);
    } else {
        CMCall(sock, Close);
    }
    goto ENDPOINT;

FAILED:
    if (res) {
        CMCall(res, Destroy);
        res = NULL;
    }
    CMCall(sock, Close);
    sock = NULL;
ENDPOINT:
    return res;
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_HttpClientGet(
    CMUTIL_HttpClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    int *status,
    long timeout)
{
    return CMCall(client, Request, "GET", headers, uri, NULL, status, timeout);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_HttpClientPost(
    CMUTIL_HttpClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_ByteBuffer *body,
    int *status,
    long timeout)
{
    return CMCall(client, Request, "POST", headers, uri, body, status, timeout);
}

CMUTIL_STATIC void CMUTIL_HttpClientDestroy(
    CMUTIL_HttpClient *client)
{
    CMUTIL_HttpClient_Internal *ih = (CMUTIL_HttpClient_Internal *)client;
    ih->memst->Free(ih);
}

static CMUTIL_HttpClient g_cmutil_http_client = {
    CMUTIL_HttpClientSetVerify,
    CMUTIL_HttpClientSetSSLCert,
    CMUTIL_HttpClientSetKeepAlive,
    CMUTIL_HttpClientRequest,
    CMUTIL_HttpClientGet,
    CMUTIL_HttpClientPost,
    CMUTIL_HttpClientDestroy
};

CMUTIL_STATIC CMBool CMUTIL_HttpClientParseUrl(
    CMUTIL_HttpClient_Internal *ih, const char *url)
{
    char buf[256];
    const char *p = strchr(url, ':');
    const char *q = NULL;
    if (p == NULL) return CMFalse;
    ih->ishttps = (strncmp(url, "https", p-url) == 0) ? CMTrue : CMFalse;
    p += 3;
    q = strchr(p, ':');
    if (q) {
        strncpy(ih->host, p, q-p);
        ih->host[q-p] = '\0';
        q++;
        p = strchr(q, '/');
        if (p) {
            strncpy(buf, q, p-q);
            buf[p-q] = '\0';
        } else {
            strcpy(buf, q);
        }
        ih->port = (int)strtol(buf, NULL, 10);
    } else {
        q = strchr(p, '/');
        if (q) {
            strncpy(ih->host, p, p-q);
            ih->host[p-q] = '\0';
        } else {
            strcpy(ih->host, p);
        }
        ih->port = ih->ishttps ? 443 : 80;
    }
    return CMTrue;
}

CMUTIL_HttpClient *CMUTIL_HttpClientCreateInternal(
    CMUTIL_Mem *memst, const char *urlprefix)
{
    CMUTIL_HttpClient_Internal *ih =
        memst->Alloc(sizeof(CMUTIL_HttpClient_Internal));
    memset(ih, 0x0, sizeof(CMUTIL_HttpClient_Internal));
    ih->base = g_cmutil_http_client;
    ih->memst = memst;
    ih->keepalive = CMTrue;
    if (!CMUTIL_HttpClientParseUrl(ih, urlprefix)) {
        return NULL;
    }
    ih->verifyhost = CMTrue;
    ih->verifypeer = CMFalse;
    return &ih->base;
}

CMUTIL_HttpClient *CMUTIL_HttpClientCreate(const char *urlprefix)
{
    return CMUTIL_HttpClientCreateInternal(CMUTIL_GetMem(), urlprefix);
}