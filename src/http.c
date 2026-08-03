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

// upper bound for a single response body / chunk, guards against
// absurd or hostile length headers.
#define CMUTIL_HTTP_MAX_BODY    (1024L*1024L*1024L)

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
    snprintf(buf, sizeof(buf), "%s:%d", host, port);
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
    snprintf(buf, sizeof(buf), "%s:%d", host, port);
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
    if ((certfile && strlen(certfile) >= sizeof(ih->certfile)) ||
        (keyfile && strlen(keyfile) >= sizeof(ih->keyfile)) ||
        (cafile && strlen(cafile) >= sizeof(ih->cafile))) {
        CMLogError("certificate file path too long");
        return CMFalse;
    }
    memset(ih->certfile, 0x0, sizeof(ih->certfile));
    memset(ih->keyfile, 0x0, sizeof(ih->keyfile));
    memset(ih->cafile, 0x0, sizeof(ih->cafile));
    if (certfile) strcpy(ih->certfile, certfile);
    if (keyfile) strcpy(ih->keyfile, keyfile);
    if (cafile) strcpy(ih->cafile, cafile);
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
    size_t hlen;
    if (p == NULL) return NULL;
    hlen = (size_t)(p - buf);
    // header name longer than the local buffer cannot match anything.
    if (hlen >= sizeof(hbuf)) return NULL;
    memcpy(hbuf, buf, hlen);
    hbuf[hlen] = '\0';
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

    snprintf(buf, sizeof(buf), "%s %s HTTP/1.1", method, uri);
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
            snprintf(buf, sizeof(buf), "%s: %s",
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
        snprintf(buf, sizeof(buf), "Host: %s:%d", ih->host, ih->port);
        sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
        if (sr != CMSocketOk) {
            CMLogError("failed to write header line");
            goto FAILED;
        }
    }
    if (!ih->keepalive) {
        snprintf(buf, sizeof(buf), "Connection: close");
    } else {
        snprintf(buf, sizeof(buf), "Connection: keep-alive");
    }
    sr = CMUTIL_HttpClientWriteLine(sock, buf, timeout);
    if (sr != CMSocketOk) {
        CMLogError("failed to write header line");
        goto FAILED;
    }
    if (strcasecmp(method, "POST") == 0 || strcasecmp(method, "PUT") == 0) {
        if (body != NULL) {
            if (needLength) {
                snprintf(buf, sizeof(buf), "Content-Length: %zu",
                    CMCall(body, GetSize));
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
            // Content-Length already delimits the body. A CRLF after it is
            // extra bytes the peer reads as the start of the next request,
            // which desynchronizes a kept-alive connection.
            sr = CMCall(sock, Write, data, size, timeout);
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
        // Only HTTP/1.1 keeps a connection open by default. An older peer
        // closes right after the response unless it asks to keep going, and
        // pooling that socket hands the next request a broken pipe.
        if ((size_t)(p - buf) != 8 || strncmp(buf, "HTTP/1.1", 8) != 0)
            keepalive = CMFalse;
        // p points at the space before the status code,
        // the reason phrase starts after the next one.
        q = strchr(p+1, ' ');
        if (q) *q = '\0';
        *status = (int)strtol(p+1, NULL, 10);
        // The whole 2xx range is success - 201 Created and 204 No Content
        // are ordinary answers to a POST or a DELETE, not something to
        // complain about.
        if (*status < 200 || *status > 299) {
            CMLogWarn("status code is not successful: %d - %s",
                      *status, q? q+1:"");
       }
    }

    // read headers
    while (CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout)) {
        if (strlen(buf) == 0) {
            break;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Connection");
        if (p) {
            if (strcasecmp(p, "close") == 0) {
                keepalive = CMFalse;
            } else if (strcasecmp(p, "keep-alive") == 0) {
                // an HTTP/1.0 peer opting in, if we asked for it too
                keepalive = ih->keepalive;
            }
            continue;
        }
        p = CMUTIL_HttpClientLineValue(buf, "Content-Length");
        if (p) {
            char *endp = NULL;
            const long clen = strtol(p, &endp, 10);
            if (endp == p || clen < 0 || clen > CMUTIL_HTTP_MAX_BODY) {
                CMLogError("invalid Content-Length: %s", p);
                goto FAILED;
            }
            len = (size_t)clen;
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
        sr = CMCall(sock, Read, res, (uint32_t)len, timeout);
        if (sr != CMSocketOk) {
            CMLogError("failed to read response body");
            goto FAILED;
        }
    } else if (strstr(transfer, "chunked")) {
        res = CMUTIL_ByteBufferCreateInternal(ih->memst, 2048);
        while (CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout)) {
            char *endp = NULL;
            long clen;
            if (*buf == '\0')
                break;
            clen = strtol(buf, &endp, 16);
            if (endp == buf || clen < 0 || clen > CMUTIL_HTTP_MAX_BODY) {
                CMLogError("invalid chunk size: %s", buf);
                goto FAILED;
            }
            if (clen == 0) {
                // consume chunk end \r\n
                CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout);
                break;
            }
            sr = CMCall(sock, Read, res, (uint32_t)clen, timeout);
            if (sr != CMSocketOk) {
                CMLogError("failed to read response body");
                goto FAILED;
            }
            // consume chunk end \r\n
            CMUTIL_HttpClientReadLine(sock, buf, sizeof(buf), timeout);
        }
    } else {
        // no body at all(Content-Length: 0, 204, 304 or HEAD response),
        // which is a perfectly valid response.
        res = CMUTIL_ByteBufferCreateInternal(ih->memst, 1);
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
    size_t hlen, plen;
    const char *p = strchr(url, ':');
    const char *q = NULL;
    if (p == NULL) return CMFalse;
    if (strncmp(p, "://", 3) != 0) {
        CMLogError("invalid url: %s", url);
        return CMFalse;
    }
    // the whole scheme must match, not only as many characters as it has.
    ih->ishttps = ((size_t)(p - url) == 5 && strncmp(url, "https", 5) == 0)?
                CMTrue:CMFalse;
    p += 3;
    q = strchr(p, ':');
    if (q) {
        hlen = (size_t)(q - p);
        if (hlen == 0 || hlen >= sizeof(ih->host)) {
            CMLogError("invalid or too long host name in url: %s", url);
            return CMFalse;
        }
        memcpy(ih->host, p, hlen);
        ih->host[hlen] = '\0';
        q++;
        p = strchr(q, '/');
        plen = p? (size_t)(p - q):strlen(q);
        if (plen == 0 || plen >= sizeof(buf)) {
            CMLogError("invalid port in url: %s", url);
            return CMFalse;
        }
        memcpy(buf, q, plen);
        buf[plen] = '\0';
        ih->port = (int)strtol(buf, NULL, 10);
        if (ih->port <= 0 || ih->port > 65535) {
            CMLogError("invalid port in url: %s", url);
            return CMFalse;
        }
    } else {
        q = strchr(p, '/');
        hlen = q? (size_t)(q - p):strlen(p);
        if (hlen == 0 || hlen >= sizeof(ih->host)) {
            CMLogError("invalid or too long host name in url: %s", url);
            return CMFalse;
        }
        memcpy(ih->host, p, hlen);
        ih->host[hlen] = '\0';
        ih->port = ih->ishttps? 443:80;
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
        memst->Free(ih);
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

/* ---------------------------------------------------------------------------
 * REST client.
 *
 * CMUTIL_RestClient starts with a CMUTIL_HttpClient by value, so a pointer to
 * one is a valid HTTP client. That rules out simply reusing the HTTP client's
 * own object: its private fields sit right after the interface, where the REST
 * methods have to go. So a REST client owns an HTTP client instead and its
 * inherited half is a set of forwarders - the layout contract of the header is
 * kept without http.c's internals leaking into it.
 * ---------------------------------------------------------------------------
 */

/* Put() stores a void*, and a string literal would have to lose its const to
 * get there. */
static char g_cmutil_rest_json_type[] = "application/json";

typedef struct CMUTIL_RestClient_Internal {
    CMUTIL_RestClient   base;
    CMUTIL_HttpClient   *http;
    CMUTIL_Mem          *memst;
    long                timeout;
    int                 status;
} CMUTIL_RestClient_Internal;

CMUTIL_STATIC CMBool CMUTIL_RestClientSetVerify(
    CMUTIL_HttpClient *client, CMBool verify_host, CMBool verify_peer)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    return CMCall(ir->http, SetVerify, verify_host, verify_peer);
}

CMUTIL_STATIC CMBool CMUTIL_RestClientSetSSLCert(
    CMUTIL_HttpClient *client,
    const char *certfile,
    const char *keyfile,
    const char *cafile)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    return CMCall(ir->http, SetSSLCert, certfile, keyfile, cafile);
}

CMUTIL_STATIC void CMUTIL_RestClientSetKeepAlive(
    CMUTIL_HttpClient *client, CMBool keepalive)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    CMCall(ir->http, SetKeepAlive, keepalive);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_RestClientHttpRequest(
    CMUTIL_HttpClient *client,
    const char *method,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_ByteBuffer *body,
    int *status,
    long timeout)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    return CMCall(ir->http, Request,
                  method, headers, uri, body, status, timeout);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_RestClientHttpGet(
    CMUTIL_HttpClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    int *status,
    long timeout)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    return CMCall(ir->http, Get, headers, uri, status, timeout);
}

CMUTIL_STATIC CMUTIL_ByteBuffer *CMUTIL_RestClientHttpPost(
    CMUTIL_HttpClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_ByteBuffer *body,
    int *status,
    long timeout)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    return CMCall(ir->http, Post, headers, uri, body, status, timeout);
}

CMUTIL_STATIC void CMUTIL_RestClientDestroy(
    CMUTIL_HttpClient *client)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    if (ir->http)
        CMCall(ir->http, Destroy);
    ir->memst->Free(ir);
}

CMUTIL_STATIC CMBool CMUTIL_RestClientHasHeader(
    CMUTIL_Map *headers, const char *name)
{
    const CMUTIL_Array *pairs;
    uint32_t i;
    if (headers == NULL) return CMFalse;
    pairs = CMCall(headers, GetPairs);
    if (pairs == NULL) return CMFalse;
    for (i=0; i<(uint32_t)CMCall(pairs, GetSize); i++) {
        const CMUTIL_MapPair *pair = CMCall(pairs, GetAt, i);
        if (strcasecmp(CMCall(pair, GetKey), name) == 0)
            return CMTrue;
    }
    return CMFalse;
}

/**
 * The caller's header map is left alone - the defaults go into a copy. The
 * copy borrows the caller's values, so it is created without a free callback.
 */
CMUTIL_STATIC CMUTIL_Map *CMUTIL_RestClientHeaders(
    CMUTIL_RestClient_Internal *ir, CMUTIL_Map *headers, CMBool hasbody)
{
    CMUTIL_Map *res = CMUTIL_MapCreateInternal(
                ir->memst, 16, CMFalse, NULL, 0.75f);
    if (headers)
        CMCall(res, PutAll, headers);
    if (!CMUTIL_RestClientHasHeader(headers, "Accept"))
        CMCall(res, Put, "Accept", g_cmutil_rest_json_type, NULL);
    if (hasbody && !CMUTIL_RestClientHasHeader(headers, "Content-Type"))
        CMCall(res, Put, "Content-Type", g_cmutil_rest_json_type, NULL);
    return res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_RestClientExchange(
    CMUTIL_RestClient_Internal *ir,
    const char *method,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_Json *data)
{
    CMUTIL_Map *hdrs;
    CMUTIL_ByteBuffer *body = NULL;
    CMUTIL_ByteBuffer *resbuf;
    CMUTIL_Json *res = NULL;

    // Request only assigns the status once it has read a status line, so a
    // request that never got that far has to leave a zero behind.
    ir->status = 0;

    if (data) {
        CMUTIL_String *sbuf = CMUTIL_StringCreateInternal(ir->memst, 128, NULL);
        const char *json;
        size_t len;
        CMCall(data, ToString, sbuf, CMFalse);
        json = CMCall(sbuf, GetCString);
        len = CMCall(sbuf, GetSize);
        body = CMUTIL_ByteBufferCreateInternal(ir->memst, len? len:1);
        CMCall(body, AddBytes, (const uint8_t*)json, (uint32_t)len);
        CMCall(sbuf, Destroy);
    }

    hdrs = CMUTIL_RestClientHeaders(ir, headers, body? CMTrue:CMFalse);
    resbuf = CMCall(ir->http, Request,
                    method, hdrs, uri, body, &ir->status, ir->timeout);
    CMCall(hdrs, Destroy);
    if (body)
        CMCall(body, Destroy);

    if (resbuf) {
        const size_t len = CMCall(resbuf, GetSize);
        if (len > 0) {
            CMUTIL_String *sbuf = CMUTIL_StringCreateInternal(
                        ir->memst, len + 1, NULL);
            const uint8_t *bytes = CMCall(resbuf, GetBytes);
            CMCall(sbuf, AddNString, (const char*)bytes, len);
            // An error response is often not JSON at all, and that is the
            // status code's story to tell - parse it quietly.
            res = CMUTIL_JsonParseInternal(ir->memst, sbuf, CMTrue);
            CMCall(sbuf, Destroy);
            if (res == NULL)
                CMLogWarn("%s %s: response body is not valid JSON",
                          method, uri);
        }
        CMCall(resbuf, Destroy);
    }
    return res;
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_RestClientGet(
    CMUTIL_RestClient *client,
    CMUTIL_Map *headers,
    const char *uri)
{
    return CMUTIL_RestClientExchange(
                (CMUTIL_RestClient_Internal*)client, "GET", headers, uri, NULL);
}

CMUTIL_STATIC CMUTIL_Json *CMUTIL_RestClientPost(
    CMUTIL_RestClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_Json *data)
{
    return CMUTIL_RestClientExchange(
                (CMUTIL_RestClient_Internal*)client, "POST", headers, uri, data);
}

CMUTIL_STATIC CMBool CMUTIL_RestClientPut(
    CMUTIL_RestClient *client,
    CMUTIL_Map *headers,
    const char *uri,
    CMUTIL_Json *data)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    CMUTIL_Json *res = CMUTIL_RestClientExchange(
                ir, "PUT", headers, uri, data);
    if (res)
        CMUTIL_JsonDestroy(res);
    return (ir->status >= 200 && ir->status < 300)? CMTrue:CMFalse;
}

CMUTIL_STATIC void CMUTIL_RestClientDelete(
    CMUTIL_RestClient *client,
    CMUTIL_Map *headers,
    const char *uri)
{
    CMUTIL_Json *res = CMUTIL_RestClientExchange(
                (CMUTIL_RestClient_Internal*)client,
                "DELETE", headers, uri, NULL);
    if (res)
        CMUTIL_JsonDestroy(res);
}

CMUTIL_STATIC void CMUTIL_RestClientSetTimeout(
    CMUTIL_RestClient *client, long timeout)
{
    CMUTIL_RestClient_Internal *ir = (CMUTIL_RestClient_Internal*)client;
    ir->timeout = timeout;
}

CMUTIL_STATIC int CMUTIL_RestClientGetStatus(
    const CMUTIL_RestClient *client)
{
    const CMUTIL_RestClient_Internal *ir =
            (const CMUTIL_RestClient_Internal*)client;
    return ir->status;
}

static CMUTIL_RestClient g_cmutil_rest_client = {
    {
        CMUTIL_RestClientSetVerify,
        CMUTIL_RestClientSetSSLCert,
        CMUTIL_RestClientSetKeepAlive,
        CMUTIL_RestClientHttpRequest,
        CMUTIL_RestClientHttpGet,
        CMUTIL_RestClientHttpPost,
        CMUTIL_RestClientDestroy
    },
    CMUTIL_RestClientGet,
    CMUTIL_RestClientPost,
    CMUTIL_RestClientPut,
    CMUTIL_RestClientDelete,
    CMUTIL_RestClientSetTimeout,
    CMUTIL_RestClientGetStatus
};

CMUTIL_RestClient *CMUTIL_RestClientCreateInternal(
    CMUTIL_Mem *memst, const char *urlprefix)
{
    CMUTIL_RestClient_Internal *ir;
    CMUTIL_HttpClient *http = CMUTIL_HttpClientCreateInternal(
                memst, urlprefix);
    if (http == NULL)
        return NULL;
    ir = memst->Alloc(sizeof(CMUTIL_RestClient_Internal));
    memset(ir, 0x0, sizeof(CMUTIL_RestClient_Internal));
    ir->base = g_cmutil_rest_client;
    ir->http = http;
    ir->memst = memst;
    ir->timeout = CMUTIL_REST_DEFAULT_TIMEOUT;
    return &ir->base;
}

CMUTIL_RestClient *CMUTIL_RestClientCreate(const char *urlprefix)
{
    return CMUTIL_RestClientCreateInternal(CMUTIL_GetMem(), urlprefix);
}