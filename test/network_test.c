//
// Created by 박성진 on 25. 12. 15..
//

#include <stdlib.h>
#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.network")

static CMBool g_running = CMTrue;

void client_handler(void* udata) {
    CMUTIL_Socket *csock = (CMUTIL_Socket *) udata;
    CMSocketResult sr = CMSocketOk;
    CMUTIL_ByteBuffer *buf = CMUTIL_ByteBufferCreateEx(1024);
    CMUTIL_String *str = CMUTIL_StringCreate();

    while (g_running) {
        sr = CMCall(csock, Read, buf, 4, 1000);
        if (sr == CMSocketOk) {
            uint8_t *p = CMCall(buf, GetBytes);
            size_t blen = CMCall(buf, GetSize);
            CMCall(str, AddNString, (char*)p, blen);
            int len = (int)strtol(CMCall(str, GetCString), NULL, 10);
            if (len == 0) break;
            CMCall(buf, Clear);
            sr = CMCall(csock, Read, buf, len, 1000);
            if (sr != CMSocketOk) break;
            p = CMCall(buf, GetBytes);
            blen = CMCall(buf, GetSize);
            CMCall(str, AddNString, (char*)p, blen);
            CMLogInfo("received from client: %04d%s", len, CMCall(str, GetCString));
            CMCall(str, AddString, " - response");
            len = (int)CMCall(str, GetSize);
            CMCall(str, InsertPrint, 0, "%04d", len);
            p = (uint8_t*)CMCall(str, GetCString);
            blen = CMCall(str, GetSize);
            sr = CMCall(csock, Write, p, blen, 1000);
            if (sr != CMSocketOk) break;
            CMLogInfo("sent to client: %s", CMCall(str, GetCString));
            CMCall(buf, Clear);
            CMCall(str, Clear);
        } else {
            break;
        }
    }
    CMCall(buf, Destroy);
    CMCall(str, Destroy);
    CMCall(csock, Close);
}

void *server_proc(void* udata) {
    CMUTIL_ServerSocket *ssock = (CMUTIL_ServerSocket*)udata;
    CMUTIL_ThreadPool *pool = CMUTIL_ThreadPoolCreate(-1, "Handler");
    while (g_running) {
        CMUTIL_Socket *csock = NULL;
        CMSocketResult sr = CMCall(ssock, Accept, &csock, 1000);
        if (sr == CMSocketOk) {
            CMUTIL_SocketAddr caddr;
            char host[128];
            int port;
            CMCall(csock, GetRemoteAddr, &caddr);
            CMUTIL_SocketAddrGet(&caddr, host, &port);
            CMLogInfo("client connected %s:%d", host, port);
            CMCall(pool, Execute, client_handler, csock);
            csock = NULL;
        } else if (sr != CMSocketTimeout)
            break;
    }
    CMCall(pool, Wait);
    CMCall(pool, Destroy);
    return udata;
}

void client_proc(void* udata) {
    CMUTIL_SocketAddr *addr = (CMUTIL_SocketAddr*) udata;
    CMUTIL_Socket *csock = CMUTIL_SocketConnectWithAddr(addr, 1000);
    CMUTIL_String *sbuf = CMUTIL_StringCreate();
    CMUTIL_ByteBuffer *buf = CMUTIL_ByteBufferCreateEx(1024);
    CMUTIL_Thread *self_thread = CMUTIL_ThreadSelf();
    const char *tname = CMCall(self_thread, GetName);
    CMSocketResult sr = CMSocketOk;

    if (csock) {
        char host[128]; int port;
        CMUTIL_SocketAddrGet(addr, host, &port);
        CMLogInfo("connected to server: %s:%d", host, port);
        for (int i=0; i<10; i++) {
            CMCall(sbuf, AddPrint, "%s: %d", tname, i);
            int len = CMCall(sbuf, GetSize);
            CMCall(sbuf, InsertPrint, 0, "%04d", len);
            uint8_t *p = (uint8_t*)CMCall(sbuf, GetCString);
            size_t blen = CMCall(sbuf, GetSize);
            sr = CMCall(csock, Write, p, blen, 1000);
            if (sr != CMSocketOk) break;
            CMLogInfo("sent to server: %s", CMCall(sbuf, GetCString));
            CMCall(sbuf, Clear);
            sr = CMCall(csock, Read, buf, 4, 1000);
            if (sr != CMSocketOk) break;
            p = CMCall(buf, GetBytes);
            blen = CMCall(buf, GetSize);
            CMCall(sbuf, AddNString, (char*)p, blen);
            CMLogInfo("received length : %s", CMCall(sbuf, GetCString));
            len = (int)strtol(CMCall(sbuf, GetCString), NULL, 10);
            CMCall(buf, Clear);
            sr = CMCall(csock, Read, buf, len, 1000);
            if (sr != CMSocketOk) break;
            p = CMCall(buf, GetBytes);
            CMCall(sbuf, AddNString, (char*)p, len);
            CMLogInfo("received from server: %04d%s", len, CMCall(sbuf, GetCString));
            CMCall(buf, Clear);
            CMCall(sbuf, Clear);
        }
        if (sr == CMSocketOk) {
            CMCall(csock, Write, "0000", 4, 1000);
        }
    }

    if (buf) CMCall(buf, Destroy);
    if (sbuf) CMCall(sbuf, Destroy);
    if (csock) CMCall(csock, Close);
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);
    CMUTIL_ServerSocket *ssock = NULL;
    CMUTIL_Thread *svr_thread = NULL;
    CMUTIL_ThreadPool *pool = CMUTIL_ThreadPoolCreate(-1, NULL);

    ssock = CMUTIL_ServerSocketCreate("0.0.0.0", 9999, 128);
    ASSERT(ssock != NULL, "ServerSocketCreate");

    svr_thread = CMUTIL_ThreadCreate(server_proc, ssock, "server");
    ASSERT(svr_thread != NULL, "Server ThreadCreate");
    CMCall(svr_thread, Start);

    CMUTIL_SocketAddr addr;
    CMUTIL_SocketAddrSet(&addr, "127.0.0.1", 9999);

    for (int i=0; i<10; i++) {
        CMCall(pool, Execute, client_proc, &addr);
    }

    CMCall(pool, Wait);

    ir = 0;
END_POINT:
    g_running = CMFalse;
    if (pool) CMCall(pool, Destroy);
    if (svr_thread) CMCall(svr_thread, Join);
    if (ssock) CMCall(ssock, Close);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}
