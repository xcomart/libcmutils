//
// Created by 박성진 on 25. 12. 11..
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "libcmutils.h"

CMUTIL_LogDefine("test.dgram")

static CMBool is_running = CMTrue;

void *dgram_server(void *udata) {
    int ir = 1;
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
                        ir = 0;
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

    return (void*)ir;
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_Thread *server = NULL;
    CMUTIL_DGramSocket *client = NULL;
    CMUTIL_ByteBuffer *buffer = CMUTIL_ByteBufferCreateEx(2048);
    CMUTIL_DGramSocket *dsock = CMUTIL_DGramSocketCreate();
    CMSocketResult sr = CMSocketOk;
    if (dsock == NULL) {
        CMLogError("datagram socket creation failed - failed");
        goto END_POINT;
    }
    CMLogInfo("datagram socket created - passed");

    CMUTIL_SocketAddr localaddr;
    sr = CMUTIL_SocketAddrSet(&localaddr, "0.0.0.0", 9898);
    if (sr != CMSocketOk) {
        CMLogError("CMUTIL_SocketAddrSet failed - failed");
        goto END_POINT;
    }
    sr = CMCall(dsock, Bind, &localaddr);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket binding failed - failed");
        goto END_POINT;
    }
    char *host = "127.0.0.1";
    int port;
    sr = CMUTIL_SocketAddrGet(&localaddr, NULL, &port);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket get address failed - failed");
        goto END_POINT;
    }
    CMLogInfo("binded address %s:%d - passed", host, port);

    server = CMUTIL_ThreadCreate(dgram_server, dsock, "dssvr");
    CMCall(server, Start);

    client = CMUTIL_DGramSocketCreate();

    CMUTIL_SocketAddrSet(&localaddr, host, port);

    sr = CMCall(client, Connect, &localaddr);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket connect failed - failed");
        goto END_POINT;
    }
    CMLogInfo("datagram socket connect - passed");
    CMUTIL_SocketAddr raddr;
    sr = CMCall(client, GetRemoteAddr, &raddr);
    if (sr != CMSocketOk) {
        CMLogError("datagram GetRemoteAddr failed - failed");
        goto END_POINT;
    }
    char rhost[1024];
    int rport;
    CMUTIL_SocketAddrGet(&raddr, rhost, &rport);
    CMLogInfo("datagram socket connected to %s:%d", rhost, rport);

    const char *send_data = "this is sample message";
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, Send, buffer, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket send failed - failed");
        goto END_POINT;
    }
    CMLogInfo("datagram socket send - passed");

    sr = CMCall(client, Recv, buffer, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket receive failed - failed");
        goto END_POINT;
    }
    CMLogInfo("datagram socket recv - passed");
    CMCall(buffer, GetBytes)[CMCall(buffer, GetSize)] = 0;
    CMLogInfo("received data: [%s]", CMCall(buffer, GetBytes));

    CMCall(client, Close); client = NULL;

    client = CMUTIL_DGramSocketCreateBind(NULL);
    send_data = "second data";
    CMCall(buffer, Clear);
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, SendTo, buffer, &localaddr, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket sendto failed - failed");
        goto END_POINT;
    }
    CMUTIL_SocketAddr secaddr;
    sr = CMCall(client, RecvFrom, buffer, &secaddr, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket recvfrom failed - failed");
        goto END_POINT;
    }
    CMCall(buffer, GetBytes)[CMCall(buffer, GetSize)] = 0;
    CMLogInfo("received data: [%s]", CMCall(buffer, GetBytes));

    CMCall(buffer, Clear);
    send_data = "!@#$$#@!";
    CMCall(buffer, AddBytes, (const uint8_t*)send_data, strlen(send_data));
    sr = CMCall(client, SendTo, buffer, &localaddr, 1000);
    if (sr != CMSocketOk) {
        CMLogError("datagram socket send failed - failed");
        goto END_POINT;
    }
    void *res = CMCall(server, Join); server = NULL;
    if (((int)res) == 0) {
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
    CMUTIL_Clear();
    return ir;
}