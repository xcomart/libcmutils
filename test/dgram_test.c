//
// Created by 박성진 on 25. 12. 11..
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.dgram")

static CMBool is_running = CMTrue;

void *dgram_server(void *udata) {
    void *res = udata;
    CMUTIL_DGramSocket *dsock = (CMUTIL_DGramSocket *)udata;
    CMUTIL_ByteBuffer *buffer = CMUTIL_ByteBufferCreateEx(2048);
    while (is_running) {
        CMUTIL_SocketAddr remote_addr;
        CMSocketResult sr =CMCall(dsock, RecvFrom, buffer, &remote_addr, 1000);
        char cbuf[2049] = {0,};
        switch (sr) {
            case CMSocketOk:
                strncat(cbuf, (char*)CMCall(buffer, GetBytes), CMCall(buffer, GetSize));
                CMLogInfo("received data: [%s]", cbuf);
                if (strcmp(cbuf, "!@#$$#@!") == 0) {
                    CMLogInfo("termination sequence received");
                    is_running = CMFalse;
                } else {
                    CMCall(buffer, InsertBytesAt, 0, (uint8_t*)"resp-", 5);
                    sr = CMCall(dsock, SendTo, buffer, &remote_addr, 1000);
                    if (sr != CMSocketOk) {
                        CMLogError("datagram socket send failed - failed");
                        res = NULL;
                        is_running = CMFalse;
                    }
                }
            case CMSocketTimeout:
                break;
            default:
                is_running = CMFalse;
        }
    }

    CMCall(buffer, Destroy);

    return res;
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_Thread *server = NULL;
    CMUTIL_DGramSocket *client = NULL;
    CMUTIL_ByteBuffer *buffer = CMUTIL_ByteBufferCreateEx(2048);
    CMUTIL_DGramSocket *dsock = CMUTIL_DGramSocketCreate();
    CMSocketResult sr = CMSocketOk;

    ASSERT(dsock != NULL, "CMUTIL_DGramSocketCreate");

    CMUTIL_SocketAddr localaddr;
    sr = CMUTIL_SocketAddrSet(&localaddr, "0.0.0.0", 9898);
    ASSERT(sr == CMSocketOk, "CMUTIL_SocketAddrSet");
    sr = CMCall(dsock, Bind, &localaddr);
    ASSERT(sr == CMSocketOk, "Bind");

    char *host = "127.0.0.1";
    int port;
    sr = CMUTIL_SocketAddrGet(&localaddr, NULL, &port);
    ASSERT(sr == CMSocketOk, "CMUTIL_SocketAddrGet");

    server = CMUTIL_ThreadCreate(dgram_server, dsock, "dssvr");
    CMCall(server, Start);

    client = CMUTIL_DGramSocketCreate();

    CMUTIL_SocketAddrSet(&localaddr, host, port);

    sr = CMCall(client, Connect, &localaddr);
    ASSERT(sr == CMSocketOk, "Connect");

    CMUTIL_SocketAddr raddr;
    sr = CMCall(client, GetRemoteAddr, &raddr);
    ASSERT(sr == CMSocketOk, "GetRemoteAddr");

    char rhost[1024];
    int rport;
    CMUTIL_SocketAddrGet(&raddr, rhost, &rport);
    CMLogInfo("datagram socket connected to %s:%d", rhost, rport);

    const char *send_data = "this is sample message";
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, Send, buffer, 1000);
    ASSERT(sr == CMSocketOk, "Send");

    sr = CMCall(client, Recv, buffer, 1000);
    ASSERT(sr == CMSocketOk, "Recv");

    CMCall(buffer, GetBytes)[CMCall(buffer, GetSize)] = 0;
    CMLogInfo("received data: [%s]", CMCall(buffer, GetBytes));
    ASSERT(strcmp((char*)CMCall(buffer, GetBytes), "resp-this is sample message") == 0,
        "Recv received data validation");
    CMCall(buffer, Clear);
    CMCall(client, Close); client = NULL;

    client = CMUTIL_DGramSocketCreateBind(NULL);
    send_data = "second data";
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, SendTo, buffer, &localaddr, 1000);
    ASSERT(sr == CMSocketOk, "SendTo");

    CMUTIL_SocketAddr secaddr;
    sr = CMCall(client, RecvFrom, buffer, &secaddr, 1000);
    ASSERT(sr == CMSocketOk, "RecvFrom");

    CMUTIL_SocketAddrGet(&secaddr, rhost, &rport);
    ASSERT(strcmp(rhost, "127.0.0.1") == 0 && rport == 9898,
        "RecvFrom address validation");

    CMCall(buffer, GetBytes)[CMCall(buffer, GetSize)] = 0;
    CMLogInfo("received data: [%s]", CMCall(buffer, GetBytes));
    ASSERT(strcmp((char*)CMCall(buffer, GetBytes), "resp-second data") == 0,
        "RecvFrom received data validation");

    CMCall(buffer, Clear);
    send_data = "!@#$$#@!";
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, SendTo, buffer, &localaddr, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket send failed - failed");
        goto END_POINT;
    }
    const void *res = CMCall(server, Join); server = NULL;
    if (res == NULL) {
        CMLogError("server returned error");
        goto END_POINT;
    }

    ir = 0;
END_POINT:
    is_running = CMFalse;
    if (server) CMCall(server, Join);
    if (client) CMCall(client, Close);
    if (dsock) CMCall(dsock, Close);
    if (buffer) CMCall(buffer, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}