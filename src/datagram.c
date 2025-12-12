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

#include "functions.h"

#include <errno.h>

#if defined(MSWIN)
#include <winsock2.h>
static WSADATA wsa_data;
#else
# include <signal.h>
# include <netinet/tcp.h>
# include <poll.h>
#endif

CMUTIL_LogDefine("cmutils.datagram")

typedef struct CMUTIL_DGramSocket_Internal {
    CMUTIL_DGramSocket  base;
    CMUTIL_Mem          *memst;
    CMUTIL_SocketAddr   laddr;
    CMUTIL_SocketAddr   raddr;
    SOCKET              sock;
    CMBool              connected;
    CMBool              binded;
    CMBool              silent;
} CMUTIL_DGramSocket_Internal;

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketBind(
        CMUTIL_DGramSocket *dsock,
        const CMUTIL_SocketAddr *saddr)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    socklen_t addrsz = sizeof(CMUTIL_SocketAddr);
    CMUTIL_SocketAddr addr;
    if (saddr) {
        int one = 1;
        setsockopt(idsock->sock, SOL_SOCKET, SO_REUSEADDR,
                (char *) &one, sizeof(one));
        addr = *saddr;
    } else {
        memset(&addr, 0x0, addrsz);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        // to get random port
        addr.sin_port = 0;
    }
    if (bind(idsock->sock,
             (struct sockaddr*)&addr,
             addrsz) == -1) {
        CMLogErrorS("bind() failed: %s", strerror(errno));
        return CMSocketBindFailed;
    }
    getsockname(idsock->sock, (struct sockaddr*)&(idsock->laddr), &addrsz);
    // win32 getsockname may only set the port number, p=0.0005.
    // ( http://msdn.microsoft.com/library/ms738543.aspx ):
    idsock->laddr.sin_addr.s_addr = addr.sin_addr.s_addr;
    idsock->laddr.sin_family = addr.sin_family;
    idsock->binded = CMTrue;
    return CMSocketOk;
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketConnect(
        CMUTIL_DGramSocket *dsock,
        const CMUTIL_SocketAddr *saddr)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected)
        CMCall(dsock, Disconnect);
    if (connect(idsock->sock, (struct sockaddr*)saddr,
                sizeof(CMUTIL_SocketAddr)) < 0) {
        CMLogErrorS("connect failed: %s", strerror(errno));
        return CMSocketConnectFailed;
    }
    memcpy(&(idsock->raddr), saddr, sizeof(CMUTIL_SocketAddr));
    idsock->connected = CMTrue;
    return CMSocketOk;
}

CMUTIL_STATIC CMBool CMUTIL_DGramSocketIsConnected(
        CMUTIL_DGramSocket *dsock)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    return idsock->connected;
}

CMUTIL_STATIC void CMUTIL_DGramSocketDisconnect(
        CMUTIL_DGramSocket *dsock)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected) {
        closesocket(idsock->sock);
        idsock->sock = INVALID_SOCKET;
        idsock->connected = CMFalse;
    }
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketGetRemoteAddr(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_SocketAddr *saddr)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected) {
        memcpy(saddr, &(idsock->raddr), sizeof(CMUTIL_SocketAddr));
        return CMSocketOk;
    } else {
        CMLogErrorS("socket not connected");
        return CMSocketNotConnected;
    }
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketGetLocalAddr(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_SocketAddr *saddr)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected || idsock->binded) {
        memcpy(saddr, &(idsock->laddr), sizeof(CMUTIL_SocketAddr));
        return CMSocketOk;
    }
    CMLogErrorS("socket not connected or not binded");
    return CMSocketNotConnected;
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketSend(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_ByteBuffer *buf,
        long timeout)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected) {
        CMSocketResult sr = CMUTIL_SocketCheckBase(
                    idsock->sock, timeout, CMFalse, CMFalse);
        if (sr == CMSocketOk) {
            const ssize_t sent = send(
                idsock->sock, CMCall(buf, GetBytes), CMCall(buf, GetSize), 0);
            if (sent < (ssize_t)CMCall(buf, GetSize)) {
                CMLogErrorS("sendto() failed: %s", strerror(errno));
                return CMSocketSendFailed;
            }
            return CMSocketOk;
        }
        return sr;
    } else {
        CMLogErrorS("socket not connected");
        return CMSocketNotConnected;
    }
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketSendTo(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_ByteBuffer *buf,
        CMUTIL_SocketAddr *saddr,
        long timeout)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    ssize_t size;
    CMSocketResult sr = CMUTIL_SocketCheckBase(
                idsock->sock, timeout, CMFalse, CMFalse);
    if (sr == CMSocketOk) {
        size = sendto(idsock->sock,
                CMCall(buf, GetBytes), CMCall(buf, GetSize),
                0, (struct sockaddr*)saddr,
                sizeof(CMUTIL_SocketAddr));
        if (size < (ssize_t)CMCall(buf, GetSize)) {
            CMLogErrorS("sendto() failed: %s", strerror(errno));
            return CMSocketSendFailed;
        }
    }
    return sr;
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketRecv(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_ByteBuffer *buf,
        long timeout)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock->connected) {
        CMUTIL_SocketAddr raddr;
        CMSocketResult sr;
        ssize_t size;

        sr = CMUTIL_SocketCheckBase(
                idsock->sock, timeout, CMTrue, CMFalse);
        if (sr != CMSocketOk)
            return sr;
        size = recv(idsock->sock, CMCall(buf, GetBytes),
            CMCall(buf, GetCapacity), 0);
        if (size < 0) {
            CMLogErrorS("recv() failed: %s", strerror(errno));
            return CMSocketReceiveFailed;
        }
        CMCall(buf, ShrinkTo, size);
        return CMSocketOk;
    }
    CMLogErrorS("socket not connected");
    return CMSocketNotConnected;
}

CMUTIL_STATIC CMSocketResult CMUTIL_DGramSocketRecvFrom(
        CMUTIL_DGramSocket *dsock,
        CMUTIL_ByteBuffer *buf,
        CMUTIL_SocketAddr *saddr,
        long timeout)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    socklen_t addrsize = sizeof(CMUTIL_SocketAddr);
    ssize_t size;
    CMSocketResult sr;

    sr = CMUTIL_SocketCheckBase(
            idsock->sock, timeout, CMTrue, CMFalse);
    if (sr != CMSocketOk)
        return sr;
    size = recvfrom(idsock->sock,
            CMCall(buf, GetBytes), CMCall(buf, GetCapacity),
            0, (struct sockaddr*)saddr, &addrsize);
    if (size < 0) {
        CMLogErrorS("recvfrom() failed: %s", strerror(errno));
        return CMSocketReceiveFailed;
    }
    CMCall(buf, ShrinkTo, (size_t)size);
    return CMSocketOk;
}

CMUTIL_STATIC void CMUTIL_DGramSocketClose(CMUTIL_DGramSocket *dsock)
{
    CMUTIL_DGramSocket_Internal *idsock = (CMUTIL_DGramSocket_Internal*)dsock;
    if (idsock) {
        if (idsock->sock != INVALID_SOCKET)
            closesocket(idsock->sock);
        idsock->memst->Free(idsock);
    }
}

static CMUTIL_DGramSocket g_cmutil_dgramsocket = {
    CMUTIL_DGramSocketBind,
    CMUTIL_DGramSocketConnect,
    CMUTIL_DGramSocketIsConnected,
    CMUTIL_DGramSocketDisconnect,
    CMUTIL_DGramSocketGetRemoteAddr,
    CMUTIL_DGramSocketGetLocalAddr,
    CMUTIL_DGramSocketSend,
    CMUTIL_DGramSocketRecv,
    CMUTIL_DGramSocketSendTo,
    CMUTIL_DGramSocketRecvFrom,
    CMUTIL_DGramSocketClose
};

CMUTIL_DGramSocket *CMUTIL_DGramSocketCreateInternal(
        CMUTIL_Mem *memst, CMUTIL_SocketAddr *addr, CMBool silent)
{
    CMSocketResult sr;
    CMUTIL_DGramSocket_Internal *res =
            memst->Alloc(sizeof(CMUTIL_DGramSocket_Internal));
    unsigned long arg;
    int rc;
    memset(res, 0x0, sizeof(CMUTIL_DGramSocket_Internal));
    memcpy(res, &g_cmutil_dgramsocket, sizeof(CMUTIL_DGramSocket));
    res->memst = memst;
    res->silent = silent;
    res->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (res->sock == INVALID_SOCKET) {
        CMLogErrorS("cannot create socket");
        goto FAILED;
    }

#if defined(SUNOS)
    rc = fcntl(res->sock, F_SETFL, fcntl(res->sock, F_GETFL, 0) | O_NONBLOCK);
    if (rc < 0) {
        if (!res->silent)
            CMLogErrorS("fnctl failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }
#else
    arg = 1;
    rc = ioctlsocket(res->sock, FIONBIO, &arg);
    if (rc < 0) {
        if (!res->silent)
            CMLogErrorS("ioctl failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }
#endif

    if (addr) {
        sr = CMCall((CMUTIL_DGramSocket*)res, Bind, addr);
        if (sr != CMSocketOk) {
            CMLogError("socket bind address failed");
            goto FAILED;
        }
    }
    return (CMUTIL_DGramSocket*)res;
FAILED:
    CMCall((CMUTIL_DGramSocket*)res, Close);
    return NULL;
}

CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreate(void)
{
    return CMUTIL_DGramSocketCreateBind(NULL);
}

CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreateBind(
        CMUTIL_SocketAddr *addr)
{
    return CMUTIL_DGramSocketCreateInternal(CMUTIL_GetMem(), addr, CMFalse);
}

