/*
                      GNU General Public License
    libcmutils is a bunch of commonly used utility functions for multiplatform.
    Copyright (C) 2016 Dennis Soungjin Park <xcomart@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "functions.h"

#include <errno.h>

#if defined(MSWIN)
static WSADATA wsa_data;
#else
# include <signal.h>
# include <netinet/tcp.h>
#endif

#if defined(CMUTIL_SUPPORT_SSL)
# if defined(CMUTIL_SSL_USE_OPENSSL)
#  include <openssl/ssl.h>
# else
#  include <gnutls/gnutls.h>
# endif
#endif

CMUTIL_LogDefine("cmutils.network")

#if defined(APPLE)
static CMUTIL_Mutex *g_cmutil_hostbyname_mutex = NULL;
struct hostent *CMUTIL_NetworkGetHostByName(const char *name)
{
    struct hostent *res = NULL;
    // lock gethostbyname function to guarantee consistancy of
    // CMUTIL_NetworkGetHostByNameR
    CMUTIL_CALL(g_cmutil_hostbyname_mutex, Lock);
    res = gethostbyname(name);
    CMUTIL_CALL(g_cmutil_hostbyname_mutex, Unlock);
    return res;
}

int CMUTIL_NetworkGetHostByNameR(const char *name,
                    struct hostent *ret, char *buf, size_t buflen,
                    struct hostent **result, int *h_errnop)
{
    int i;
    struct hostent *staticval = NULL;
    register char *p = buf, *q;
    size_t clen;

#define declen(sz) do {                                 \
    if (buflen - sz - 1 > 0) buflen -= sz;              \
    else {                                              \
        CMUTIL_CALL(g_cmutil_hostbyname_mutex, Unlock); \
        return -1;                                      \
    } } while(0)

    // clear given memory buffer
    memset(buf, 0x0, buflen);

    // lock gethostbyname function to guarantee consistancy.
    CMUTIL_CALL(g_cmutil_hostbyname_mutex, Lock);
    staticval = gethostbyname(name);
    memcpy(ret, staticval, sizeof(struct hostent));
    clen = sizeof(char*) * staticval->h_length;

    // reserve space for address pointers
    ret->h_addr_list = (char**)p;
    p += clen; declen(clen);

    // reserve space for alias pointers
    ret->h_aliases = (char**)p;
    p += clen; declen(clen);

    // copy each data to result structure
    for (i=0; i<staticval->h_length; i++) {

        q = staticval->h_addr_list[i];
        ret->h_addr_list[i] = p;
        while (*q) {
            *p++ = *q++;
            declen(1);
        }
        *p++ = 0x0; declen(1);

        q = staticval->h_aliases[i];
        ret->h_aliases[i] = p;
        while (*q) {
            *p++ = *q++;
            declen(1);
        }
        *p++ = 0x0; declen(1);
    }

    // copy name
    ret->h_name = p;
    q = staticval->h_name;
    while (*q) {
        *p++ = *q++;
        declen(1);
    }
    *p++ = 0x0; declen(1);

    CMUTIL_CALL(g_cmutil_hostbyname_mutex, Unlock);
    CMUTIL_UNUSED(result, h_errnop);
    return 0;
}

#endif

void CMUTIL_NetworkInit()
{
#if defined(MSWIN)
    WSAStartup(MAKEWORD(2,2), &wsa_data);
#elif defined(APPLE)
    g_cmutil_hostbyname_mutex = CMUTIL_MutexCreate();
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

void CMUTIL_NetworkClear()
{
#if defined(MSWIN)
    WSACleanup();
#elif defined(APPLE)
    CMUTIL_CALL(g_cmutil_hostbyname_mutex, Destroy);
#endif
}

typedef struct CMUTIL_Socket_Internal {
    CMUTIL_Socket		base;
    SOCKET				sock;
    struct sockaddr_in	peer;
    CMUTIL_Bool			silent;
    CMUTIL_Mem          *memst;
} CMUTIL_Socket_Internal;

CMUTIL_STATIC CMUTIL_Socket_Internal *CMUTIL_SocketCreate(
        CMUTIL_Mem *memst, CMUTIL_Bool silent);

# define CMUTIL_RBUF_LEN    1024
CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketRead(
        const CMUTIL_Socket *sock, CMUTIL_String *buffer, uint32_t size, long timeout)
{
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    int width, rc;
    uint32_t rsize;
    fd_set rfds;
    struct timeval tv;
    char buf[CMUTIL_RBUF_LEN];

    /*
      Kernel object handles are process specific.
      That is, a process must either create the object
      or open an existing object to obtain a kernel object handle.
      The per-process limit on kernel handles is 2^24.
      So SOCKET can be casted to 32bit integer in 64bit windows.
    */
    width = (int)(isock->sock + 1);
    rsize = 0;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    while (size > rsize) {
        uint32_t toberead = size - rsize;
        FD_ZERO(&rfds);
        FD_SET(isock->sock, &rfds);

        if (toberead > CMUTIL_RBUF_LEN)
            toberead = CMUTIL_RBUF_LEN;

        rc = select(width, &rfds, NULL, NULL, &tv);
        if (rc < 0) return CMUTIL_SocketSelectFailed;
        else if (rc == 0) return CMUTIL_SocketTimeout;

#if defined(LINUX)
        rc = (int)recv(isock->sock, buf, toberead, MSG_NOSIGNAL);
#else
        rc = (int)recv(isock->sock, buf, toberead, 0);
#endif
        if (rc == SOCKET_ERROR) {
            if (!isock->silent)
                CMLogError("recv failed.(%d:%s)", errno, strerror(errno));
            return CMUTIL_SocketReceiveFailed;
        } else if (rc == 0) {
            if (!isock->silent)
                CMLogError("socket read timeout.");
            return CMUTIL_SocketTimeout;
        }

        CMCall(buffer, AddNString, buf, (uint32_t)rc);
        rsize += (uint32_t)rc;
    }
    return CMUTIL_SocketOk;
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_SocketReadSocket(
        const CMUTIL_Socket *sock, long timeout, CMUTIL_SocketResult *rval)
{
    CMUTIL_Socket_Internal *res = NULL;
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    SOCKET rsock = INVALID_SOCKET;
#if defined(MSWIN)
    WSAPROTOCOL_INFO pi;
    CMUTIL_String *rcvbuf = CMUTIL_StringCreateInternal(
                isock->memst, sizeof(pi), NULL);
    *rval = CMUTIL_CALL(sock, Read, rcvbuf, sizeof(pi), timeout);
    if (*rval != CMUTIL_SocketOk) {
        // error code has been set already.
        CMUTIL_CALL(rcvbuf, Destroy);
        return NULL;
    }
    memcpy(&pi, CMUTIL_CALL(rcvbuf, GetCString), sizeof(pi));
    CMUTIL_CALL(rcvbuf, Destroy);
    rsock = WSASocket(AF_INET, SOCK_STREAM, 0, &pi, 0, WSA_FLAG_OVERLAPPED);
    if (rsock == INVALID_SOCKET)
        *rval = CMUTIL_SocketUnknownError;
#else
    char buf[5];
    size_t bufsiz = 5;
    struct iovec vector;
    struct msghdr msg;
# if !defined(SUNOS)
    struct cmsghdr *cmsg=NULL;
# endif
    int rc;

    vector.iov_base = buf;
    vector.iov_len = bufsiz;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vector;
    msg.msg_iovlen = 1;

# if !defined(SUNOS)
    cmsg = isock->memst->Alloc(sizeof(struct cmsghdr) + sizeof(SOCKET));
    cmsg->cmsg_len = sizeof(struct cmsghdr) + sizeof(SOCKET);
    msg.msg_control = cmsg;
    msg.msg_controllen = cmsg->cmsg_len;
# else
    msg.msg_accrights = (char *) &rsock;
    msg.msg_accrightslen = sizeof(SOCKET);
# endif

    // value of buf and sock may be unchanged
    // initialize these variable before recvmsg
    memset(buf, 0, bufsiz);

    // reset error
    errno = 0;
    *rval = CMCall(sock, CheckReadBuffer, timeout);

    if (*rval == CMUTIL_SocketOk) {
        rc = (int)recvmsg(isock->sock, &msg, 0);
        if (rc <= 0) {	// guess zero return is error
# if !defined(SUNOS)
            if (cmsg) isock->memst->Free(cmsg);
# endif
            if (!isock->silent)
                CMLogError("recvmsg failed.(%d:%s)", errno, strerror(errno));
            *rval = CMUTIL_SocketReceiveFailed;
            return NULL;
        }

        *(buf+4) = 0x00;	// null terminate
# if !defined(SUNOS)
        // receive open file descriptor from parent
        memcpy(&rsock, CMSG_DATA(cmsg), sizeof(SOCKET));
        isock->memst->Free(cmsg);
# endif
    } else if (*rval == CMUTIL_SocketTimeout) {
        if (!isock->silent)
            CMLogError("socket read timeout");
    } else {
        if (!isock->silent)
            CMLogError("socket receive failed.(%d:%s)", errno, strerror(errno));
    }
#endif
    if (*rval == CMUTIL_SocketOk && rsock != INVALID_SOCKET) {
        struct sockaddr_storage addr;
        socklen_t ssize = (socklen_t)sizeof(addr);
        getpeername(rsock, (struct sockaddr*)&addr, &ssize);
        if (addr.ss_family == AF_INET) {
            res = CMUTIL_SocketCreate(isock->memst, isock->silent);
            res->sock = rsock;
            memcpy(&(res->peer), &addr, sizeof(struct sockaddr_in));
        } else {
            // TODO: IPv6
            if (!isock->silent)
                CMLogError("IPv6 not supported yet.");
            *rval = CMUTIL_SocketUnsupported;
        }
    }
    return (CMUTIL_Socket*)res;
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketWrite(
        const CMUTIL_Socket *sock, CMUTIL_String *data, long timeout)
{
    uint32_t length = (uint32_t)CMCall(data, GetSize);
    return CMCall(sock, WritePart, data, 0, length, timeout);
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketWritePart(
        const CMUTIL_Socket *sock, CMUTIL_String *data,
        int offset, uint32_t length, long timeout)
{
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    int width, rc;
    uint32_t size = length;
    fd_set wfds;
    struct timeval tv;

    /*
    Kernel object handles are process specific.
    That is, a process must either create the object
    or open an existing object to obtain a kernel object handle.
    The per-process limit on kernel handles is 2^24.
    So SOCKET can be casted to 32bit integer in 64bit windows.
    */
    width = (int)(isock->sock + 1);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    while (size > 0) {
        FD_ZERO(&wfds);
        FD_SET(isock->sock, &wfds);

        rc = select(width, NULL, &wfds, NULL, &tv);
        if (rc < 0) {
            if (!isock->silent)
                CMLogError("select failed.(%d:%s)", errno, strerror(errno));
            return CMUTIL_SocketSelectFailed;
        }
        else if (rc == 0) return CMUTIL_SocketTimeout;

#if defined(LINUX)
        rc = (int)send(isock->sock, CMCall(data, GetCString) + offset,
                       size, MSG_NOSIGNAL);
#else
        rc = (int)send(isock->sock, CMUTIL_CALL(data, GetCString) + offset,
                       size, 0);
#endif
        if (rc == SOCKET_ERROR) {
            if (errno == EAGAIN
#if !defined(MSWIN)
                || errno == EWOULDBLOCK
#endif
                )
                continue;
            if (!isock->silent)
                CMLogError("socket write error.(%d:%s)",
                           errno, strerror(errno));
            return CMUTIL_SocketSendFailed;
        }
        offset += rc;
        size -= (uint32_t)rc;
    }

    return CMUTIL_SocketOk;
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketWriteSocket(
        const CMUTIL_Socket *sock,
        CMUTIL_Socket *tobesent, pid_t pid, long timeout)
{
    // TODO:
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    CMUTIL_Socket_Internal *isent = (CMUTIL_Socket_Internal*)tobesent;
    CMUTIL_SocketResult res;
#if defined(MSWIN)
    int ir;
    WSAPROTOCOL_INFO pi;
    CMUTIL_String *sendbuf = NULL;

    ir = WSADuplicateSocket(isent->sock, pid, &pi);
    if (ir != 0)
        return CMUTIL_SocketUnknownError;
    sendbuf = CMUTIL_StringCreateInternal(isock->memst, sizeof(pi), NULL);
    CMUTIL_CALL(sendbuf, AddNString, (char*)&pi, sizeof(pi));

    res = CMUTIL_CALL(&isock->base, Write, sendbuf, timeout);
    CMUTIL_CALL(sendbuf, Destroy);
#else
    char *cmd="SERV";
    struct iovec vector;
    struct msghdr msg;
#if !defined(SUNOS)
    struct cmsghdr *cmsg=NULL;
#endif
    int rc;

    CMUTIL_UNUSED(pid);

    vector.iov_base = cmd;
    vector.iov_len = strlen(cmd);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vector;
    msg.msg_iovlen = 1;

#if !defined(SUNOS)
    cmsg = isock->memst->Alloc(sizeof(struct cmsghdr) + sizeof(SOCKET));
    cmsg->cmsg_len = sizeof(struct cmsghdr) + sizeof(SOCKET);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(cmsg), &(isent->sock), sizeof(SOCKET));

    msg.msg_control = cmsg;
    msg.msg_controllen = cmsg->cmsg_len;
#else
    msg.msg_accrights = (char *) &(isent->sock);
    msg.msg_accrightslen = sizeof(SOCKET);
#endif

    // reset error
    errno = 0;
    res = CMCall(sock, CheckWriteBuffer, timeout);
    if (res != CMUTIL_SocketOk) {
#if !defined(SUNOS)
        isock->memst->Free(cmsg);
#endif
        if (!isock->silent) {
            if (res == CMUTIL_SocketTimeout) {
                CMLogError("socket send timeout.");
            } else {
                CMLogError("socket send error.(%d:%s)", errno, strerror(errno));
            }
        }
        return res;
    }

#if defined(LINUX)
    rc = (int)sendmsg(isock->sock, &msg, MSG_NOSIGNAL);
#else
    rc = (int)sendmsg(isock->sock, &msg, 0);
#endif
#if !defined(SUNOS)
    isock->memst->Free(cmsg);
#endif
    if (rc <= 0) {	// guess zero return is error
        if (!isock->silent)
            CMLogError("sendmsg error.(%d:%s)", errno, strerror(errno));
        return CMUTIL_SocketSendFailed;
    }
#endif
    return res;
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketCheckBuffer(
        const CMUTIL_Socket *sock, long timeout, CMUTIL_Bool isread)
{
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    int rc, width;
    struct timeval tv;
    fd_set fds;

    width = (int)(isock->sock + 1);

    memset(&tv, 0x0, sizeof(struct timeval));
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    FD_ZERO(&fds);
    FD_SET(isock->sock, &fds);

    if (isread)
        rc = select(width, &fds, NULL, NULL, &tv);
    else
        rc = select(width, NULL, &fds, NULL, &tv);
    if (rc < 0) {
        if (!isock->silent)
            CMLogError("select failed.(%d:%s)", errno, strerror(errno));
        return CMUTIL_SocketSelectFailed;
    } else if (rc == 0) {
        return CMUTIL_SocketTimeout;
    } else {
        return CMUTIL_SocketOk;
    }
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketCheckReadBuffer(
        const CMUTIL_Socket *sock, long timeout)
{
    return CMUTIL_SocketCheckBuffer(sock, timeout, CMTrue);
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketCheckWriteBuffer(
        const CMUTIL_Socket *sock, long timeout)
{
    return CMUTIL_SocketCheckBuffer(sock, timeout, CMFalse);
}

CMUTIL_STATIC void CMUTIL_SocketGetRemoteAddr(
        const CMUTIL_Socket *sock, char *hostbuf, int *port)
{
    const CMUTIL_Socket_Internal *isock = (const CMUTIL_Socket_Internal*)sock;
    unsigned long laddr;
    *port = (int)ntohs(isock->peer.sin_port);
    laddr = ntohl(isock->peer.sin_addr.s_addr);
    sprintf(hostbuf, "%u.%u.%u.%u",
            (unsigned int)((laddr >> 24) & 0xFF),
            (unsigned int)((laddr >> 16) & 0xFF),
            (unsigned int)((laddr >>  8) & 0xFF),
            (unsigned int)((laddr >>  0) & 0xFF));
}

CMUTIL_STATIC void CMUTIL_SocketClose(CMUTIL_Socket *sock)
{
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
    if (isock) {
        if (isock->sock != INVALID_SOCKET)
            closesocket(isock->sock);
        isock->memst->Free(isock);
    }
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_SocketConnectByIP(
        unsigned char *ip, int port,
        CMUTIL_Socket_Internal *is, long timeout, int retrycnt,
        CMUTIL_Bool silent)
{
    uint32_t addr;
    struct sockaddr_in *serv = &(is->peer);
    SOCKET s;
    int rc;
#if defined(MSWIN)
    u_long arg;
#else
    uint64_t arg;
#endif
    int iarg;
    int width;
    fd_set wfds;
    struct timeval tv;

CONNECT_RETRY:

    memset(serv, 0x0, sizeof(struct sockaddr_in));
    serv->sin_family = AF_INET;
    serv->sin_port = htons((uint16_t) port);
    addr = (uint32_t)
        ((uint32_t) ip[0] << 24L) | ((uint32_t) ip[1] << 16L) |
        ((uint32_t) ip[2] <<  8L) | ((uint32_t) ip[3]);
    serv->sin_addr.s_addr = htonl(addr);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        if (!silent)
            CMLogError("cannot create socket.(%d:%s)", errno, strerror(errno));
        return CMFalse;
    }

    iarg = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *) &iarg, sizeof(iarg));
    if (rc < 0) {
        if (!silent)
            CMLogWarn("SO_KEEPALIVE failed.(%d:%s) ignored.",
                      errno, strerror(errno));
    }

    iarg = 1;
    rc = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*) &iarg, sizeof(iarg));
    if (rc < 0) {
        if (!silent)
            CMLogWarn("TCP_NODELAY failed.(%d:%s) ignored.",
                      errno, strerror(errno));
    }

#if defined(SUNOS)
    rc = fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    if (rc < 0) {
        if (!silent)
            CMLogError("fcntl failed.(%d:%s)", errno, strerror(errno));
        closesocket(s);
        return CMFalse;
    }
#else
    arg = 1;
    rc = ioctlsocket(s, (long)FIONBIO, &arg);
    if (rc < 0) {
        if (!silent)
            CMLogError("ioctl failed.(%d:%s)", errno, strerror(errno));
        closesocket(s);
        return CMFalse;
    }
#endif
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    rc = connect(s, (struct sockaddr *) &serv, sizeof(serv));
    if (rc < 0) {

#if defined(MSWIN)
        if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
        if (errno == EWOULDBLOCK || errno == EINPROGRESS)
#endif
        {
            /*
            Kernel object handles are process specific.
            That is, a process must either create the object
            or open an existing object to obtain a kernel object handle.
            The per-process limit on kernel handles is 2^24.
            So SOCKET can be casted to 32bit integer in 64bit windows.
            */
            width = (int)(s + 1);

            FD_ZERO(&wfds);
            FD_SET(s, &wfds);

            rc = select(width, NULL, &wfds, NULL, &tv);

            if (rc > 0) {		// OK, connected to server
                is->sock = s;
                return CMTrue;
            } else if (rc == 0) {
                if (!silent)
                    CMLogError("connent timeout.");
                closesocket(s);
                // connect timed out
                return CMFalse;
            }
            // fall through to retry to connect
        }

        closesocket(s);

        if (retrycnt > 0) {
            retrycnt = retrycnt - 1;
            goto CONNECT_RETRY;
        }
        if (!silent)
            CMLogError("connect failed.(%d:%s)", errno, strerror(errno));
        return CMFalse;
    }

    is->sock = s;
    return CMTrue;
}

static CMUTIL_Socket g_cmutil_socket = {
    CMUTIL_SocketRead,
    CMUTIL_SocketWrite,
    CMUTIL_SocketWritePart,
    CMUTIL_SocketCheckReadBuffer,
    CMUTIL_SocketCheckWriteBuffer,
    CMUTIL_SocketReadSocket,
    CMUTIL_SocketWriteSocket,
    CMUTIL_SocketGetRemoteAddr,
    CMUTIL_SocketClose
};

CMUTIL_STATIC CMUTIL_Socket_Internal *CMUTIL_SocketCreate(
        CMUTIL_Mem *memst, CMUTIL_Bool silent)
{
    CMUTIL_Socket_Internal *res = NULL;
    res = memst->Alloc(sizeof(CMUTIL_Socket_Internal));
    memset(res, 0x0, sizeof(CMUTIL_Socket_Internal));
    res->sock = INVALID_SOCKET;
    res->silent = silent;
    res->memst = memst;
    memcpy(res, &g_cmutil_socket, sizeof(CMUTIL_Socket));
    return res;
}

CMUTIL_Bool CMUTIL_SocketConnectBase(
        CMUTIL_Socket_Internal *res, const char *host, int port, long timeout,
        CMUTIL_Bool silent)
{
    uint8_t ip[4];
    uint32_t in[4];
    int rc, i;

    rc = sscanf(host, "%u.%u.%u.%u", &(in[0]), &(in[1]), &(in[2]), &(in[3]));
    if (rc == 4) {
        for (i=0 ; i<4 ; i++) {
            if (in[i] > 255) return CMFalse;
            ip[i] = (uint8_t)in[i];
        }
        if (!CMUTIL_SocketConnectByIP(ip, port, res, timeout, 1, silent)) {
            return CMFalse;
        }

    } else {
        char buf[2048];

        struct hostent hp;

        if (gethostbyname_r(host, &hp, buf, sizeof(buf), NULL, NULL) == 0)
            return CMFalse;

        if ((short) hp.h_addrtype != AF_INET)
            return CMFalse;

        for (i=0 ; hp.h_addr_list[i] != NULL ; i++)
        {
            memcpy(ip, hp.h_addr_list[i], 4);

            if (CMUTIL_SocketConnectByIP(ip, port, res, timeout, 0, silent))
                break;
        }

        if (hp.h_addr_list[i] == NULL)
            return CMFalse;
#if defined(MSWIN)
        CMUTIL_UNUSED(buf);
#endif
    }

    return CMTrue;
}

CMUTIL_Socket *CMUTIL_SocketConnectInternal(
        CMUTIL_Mem *memst, const char *host, int port, long timeout,
        CMUTIL_Bool silent)
{
    CMUTIL_Socket_Internal *res = CMUTIL_SocketCreate(memst, silent);
    if (CMUTIL_SocketConnectBase(res, host, port, timeout, silent)) {
        return (CMUTIL_Socket*)res;
    } else {
        CMCall((CMUTIL_Socket*)res, Close);
        return NULL;
    }
}

CMUTIL_Socket *CMUTIL_SocketConnect(
        const char *host, int port, long timeout)
{
    return CMUTIL_SocketConnectInternal(
                CMUTIL_GetMem(), host, port, timeout, CMFalse);
}

typedef struct CMUTIL_ServerSocket_Internal {
    CMUTIL_ServerSocket	base;
    SOCKET				ssock;
    CMUTIL_Bool			silent;
    CMUTIL_Mem          *memst;
} CMUTIL_ServerSocket_Internal;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ServerSocketAcceptInternal(
        const CMUTIL_ServerSocket *ssock, CMUTIL_Socket *sock, long timeout)
{
    const CMUTIL_ServerSocket_Internal *issock =
            (const CMUTIL_ServerSocket_Internal*)ssock;
    int width, rc;
    fd_set rfds;
    socklen_t len;
    struct timeval tv;
    CMUTIL_Socket_Internal *res = (CMUTIL_Socket_Internal*)sock;

    width = (int)(issock->ssock + 1);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(issock->ssock, &rfds);

    rc = select(width, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        if (!issock->silent)
            CMLogErrorS("select failed.(%d:%s)", errno, strerror(errno));
        return CMFalse;
    }

    len = sizeof(struct sockaddr_in);
    res->sock = accept(issock->ssock, (struct sockaddr*)&res->peer, &len);
    if (res->sock == INVALID_SOCKET) {
        if (!issock->silent)
            CMLogErrorS("accept failed.(%d:%s)", errno, strerror(errno));
        return CMFalse;
    }

    return CMTrue;
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_ServerSocketAccept(
        const CMUTIL_ServerSocket *ssock, long timeout)
{
    const CMUTIL_ServerSocket_Internal *issock =
            (const CMUTIL_ServerSocket_Internal*)ssock;
    CMUTIL_Socket_Internal *res = CMUTIL_SocketCreate(
                issock->memst, issock->silent);

    if (CMUTIL_ServerSocketAcceptInternal(ssock, (CMUTIL_Socket*)res, timeout))
        return (CMUTIL_Socket*)res;
    else {
        CMCall((CMUTIL_Socket*)res, Close);
        return NULL;
    }
}

CMUTIL_STATIC void CMUTIL_ServerSocketClose(CMUTIL_ServerSocket *ssock)
{
    CMUTIL_ServerSocket_Internal *issock = (CMUTIL_ServerSocket_Internal*)ssock;
    if (issock) {
        if (issock->ssock != INVALID_SOCKET)
            closesocket(issock->ssock);
        issock->memst->Free(issock);
    }
}

static CMUTIL_ServerSocket g_cmutil_serversocket = {
    CMUTIL_ServerSocketAccept,
    CMUTIL_ServerSocketClose
};

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ServerSocketCreateBase(
        CMUTIL_ServerSocket_Internal *res, const char *host, int port, int qcnt)
{
    struct sockaddr_in addr;
    unsigned short s_port = (unsigned short)port;
    SOCKET sock = INVALID_SOCKET;
    int rc, one=1;
    unsigned long arg;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);

    if (strcasecmp(host, "INADDR_ANY") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (isdigit((int)*host)) {
        addr.sin_addr.s_addr = inet_addr(host);
    } else {
        goto FAILED;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        if (!res->silent)
            CMLogErrorS("cannot create socket", errno, strerror(errno));
        goto FAILED;
    }

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));
    if (rc == SOCKET_ERROR) {
        if (!res->silent)
            CMLogErrorS("setsockopt failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

#if defined(SUNOS)
    rc = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    if (rc < 0) {
        if (!res->silent)
            CMLogErrorS("fnctl failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }
#else
    arg = 1;
    rc = ioctlsocket(sock, FIONBIO, &arg);
    if (rc < 0) {
        if (!res->silent)
            CMLogErrorS("ioctl failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }
#endif

    rc = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    if (rc == SOCKET_ERROR) {
        if (!res->silent)
            CMLogErrorS("bind failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    rc = listen(sock, qcnt);
    if (rc == SOCKET_ERROR) {
        if (!res->silent)
            CMLogErrorS("listen failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    res->ssock = sock;
    return CMTrue;

FAILED:
    if (sock != INVALID_SOCKET)
        closesocket(sock);
    return CMFalse;
}

CMUTIL_ServerSocket *CMUTIL_ServerSocketCreateInternal(
        CMUTIL_Mem *memst, const char *host, int port, int qcnt,
        CMUTIL_Bool silent)
{
    CMUTIL_ServerSocket_Internal *res =
            memst->Alloc(sizeof(CMUTIL_ServerSocket_Internal));
    memset(res, 0x0, sizeof(CMUTIL_ServerSocket_Internal));
    res->memst = memst;
    res->silent = silent;
    memcpy(res, &g_cmutil_serversocket, sizeof(CMUTIL_ServerSocket));
    if (CMUTIL_ServerSocketCreateBase(res, host, port, qcnt)) {
        return (CMUTIL_ServerSocket*)res;
    } else {
        CMCall((CMUTIL_ServerSocket*)res, Close);
        return NULL;
    }
}

CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
        const char *host, int port, int qcnt)
{
    return CMUTIL_ServerSocketCreateInternal(
                CMUTIL_GetMem(), host, port, qcnt, CMFalse);
}

#if defined(CMUTIL_SUPPORT_SSL)
# if defined(CMUTIL_SSL_USE_OPENSSL)
#  include <openssl/ssl.h>
# else
#  include <gnutls/gnutls.h>
# endif // !CMUTIL_SSL_USE_OPENSSL

typedef struct CMUTIL_SSLServerSocket_Internal {
    CMUTIL_ServerSocket_Internal    base;
#if defined(CMUTIL_SSL_USE_OPENSSL)
    SSL_CTX                         *sslctx;
#else
    gnutls_certificate_credentials_t cred;
    gnutls_priority_t               priority_cache;
#endif  // !CMUTIL_SSL_USE_OPENSSL
} CMUTIL_SSLServerSocket_Internal;

typedef struct CMUTIL_SSLSocket_Internal {
    CMUTIL_Socket_Internal  base;
#if defined(CMUTIL_SSL_USE_OPENSSL)
    SSL                     *session;
    // for client side certification
    SSL_CTX                 sslctx;
#else
    gnutls_session_t        session;
    // for client side certification
    gnutls_certificate_credentials_t cred;
# if GNUTLS_VERSION_NUMBER < 0x030506
    gnutls_dh_params_t      dh_params;
# endif
#endif  // !CMUTIL_SSL_USE_OPENSSL
    // for server peer connection
    CMUTIL_SSLServerSocket_Internal *server;
} CMUTIL_SSLSocket_Internal;

CMUTIL_STATIC CMUTIL_SSLSocket_Internal *CMUTIL_SSLSocketCreate(
        CMUTIL_Mem *memst, CMUTIL_Bool silent);

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SSLSocketCheckReadBuffer(
        const CMUTIL_Socket *sock, long timeout)
{
    const CMUTIL_SSLSocket_Internal *si =
            (const CMUTIL_SSLSocket_Internal*)sock;
    int width, ir;
    fd_set rfds;
    struct timeval tv;
    SOCKET fd;

#if defined(CMUTIL_SSL_USE_OPENSSL)
    fd = (SOCKET)SSL_get_fd(session->session);
    if (SSL_pending(si->session) > 0)
        return CMUTIL_SocketOk;
#else
    fd = si->base.sock;
    if (gnutls_record_check_pending(si->session) > 0)
        return CMUTIL_SocketOk;
#endif  // !CMUTIL_SSL_USE_OPENSSL

    width = fd + 1;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ir = select(width, &rfds, NULL, NULL, &tv);
    if (ir < 0) {
        return CMUTIL_SocketSelectFailed;
    } else if (ir == 0) {
        return CMUTIL_SocketTimeout;
    } else {
        return CMUTIL_SocketOk;
    }
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SSLSocketCheckWriteBuffer(
        const CMUTIL_Socket *sock, long timeout)
{
    const CMUTIL_SSLSocket_Internal *si =
            (const CMUTIL_SSLSocket_Internal*)sock;
    int width, ir;
    fd_set wfds;
    struct timeval tv;
    SOCKET fd;

#if defined(CMUTIL_SSL_USE_OPENSSL)
    fd = (SOCKET)SSL_get_fd(session->session);
#else
    fd = si->base.sock;
#endif  // !CMUTIL_SSL_USE_OPENSSL

    width = fd + 1;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ir = select(width, NULL, &wfds, NULL, &tv);
    if (ir < 0) {
        return CMUTIL_SocketSelectFailed;
    } else if (ir == 0) {
        return CMUTIL_SocketTimeout;
    } else {
        return CMUTIL_SocketOk;
    }
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SSLSocketRead(
        const CMUTIL_Socket *sock, CMUTIL_String *buffer, uint32_t size, long timeout)
{
    const CMUTIL_SSLSocket_Internal *isck =
            (const CMUTIL_SSLSocket_Internal*)sock;
    CMUTIL_SocketResult res;
    char buf[1024];
    int rc, cnt = 0;
    size_t rsz;
#if defined(SSL_USE_OPENSSL)
    int in_init=0;

    while (size > 0 && cnt < timeout * 10) {
        if (SSL_in_init(isck->session) &&
                !SSL_total_renegotiations(isck->session)) {
            in_init = 1;
        } else {
            if (in_init)
                in_init = 0;
        }

        res = CMUTIL_CALL(sock, CheckReadBuffer, timeout);
        if (res == CMUTIL_SocketOk) {
            rsz = size > 1024? 1024:size;
            rc = SSL_read(isck->session, buf, rsz);

            switch (SSL_get_error(isck->session, rc))
            {
            case SSL_ERROR_NONE:
                if (rc <= 0) {
                    CMLogError("SSL read failed");
                    return CMUTIL_SocketReceiveFailed;
                }
                CMUTIL_CALL(buffer, AddNString, buf, rc);
                size -= rc;
                if (size == 0)
                    return CMUTIL_SocketOk;
                break;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_X509_LOOKUP:
                cnt++;
                usleep(100000);
                break;
            case SSL_ERROR_ZERO_RETURN:
                CMLogError("peer closed connection");
                return CMUTIL_SocketReceiveFailed;
            default:
                CMLogError("SSL read failed");
                return CMUTIL_SocketReceiveFailed;
            }
        } else {
            CMLogError("CheckReadBuffer() failed");
            return res;
        }

    }
#else

    while (size > 0 && cnt < timeout * 10) {
        int handshake_res = 0;

        res = CMCall(sock, CheckReadBuffer, timeout);
        if (res != CMUTIL_SocketOk) {
            CMLogError("CheckReadBuffer() failed");
            return res;
        }

        rsz = size > 1024? 1024:size;
        rc = (int)gnutls_record_recv(isck->session, buf, rsz);
        switch (rc) {
        case GNUTLS_E_REHANDSHAKE:
            do {
                handshake_res = gnutls_handshake(isck->session);
            } while (handshake_res < 0 &&
                     gnutls_error_is_fatal(handshake_res) == 0);
            if (handshake_res < 0) {
                CMLogError("handshake failed : %s", gnutls_strerror(rc));
                return CMUTIL_SocketReceiveFailed;
            }
            break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
            cnt++;
            usleep(100000);
            break;
        default:
            if (rc < 0) {
                if (gnutls_error_is_fatal(rc) == 0) {
                    CMLogWarn("Warning : %s", gnutls_strerror(rc));
                } else {
                    CMLogError("receive failed : %s", gnutls_strerror(rc));
                    return CMUTIL_SocketReceiveFailed;
                }
            } else if (rc == 0) {
                CMLogError("peer closed connection");
                return CMUTIL_SocketReceiveFailed;
            } else {
                CMCall(buffer, AddNString, buf, (uint32_t)rc);
                size -= (uint32_t)rc;
            }
        }
    }
#endif  // !CMUTIL_SSL_USE_OPENSSL
    if (cnt < timeout * 10)
        return CMUTIL_SocketOk;
    else {
        CMLogError("receive timed out.");
        return CMUTIL_SocketTimeout;
    }
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_SSLSocketReadSocket(
        const CMUTIL_Socket *sock, long timeout, CMUTIL_SocketResult *rval)
{
    CMUTIL_UNUSED(sock, timeout, rval);
    CMLogError("socket passing not allowed in SSL socket");
    *rval = CMUTIL_SocketReceiveFailed;
    return NULL;
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SSLSocketWritePart(
        const CMUTIL_Socket *sock, CMUTIL_String *data,
        int offset, uint32_t length, long timeout)
{
    const CMUTIL_SSLSocket_Internal *isck =
            (const CMUTIL_SSLSocket_Internal*)sock;
    CMUTIL_SocketResult res;
    int rc, cnt = 0;
    uint32_t size = length;
    const char *buf = CMCall(data, GetCString) + offset;
#if defined(CMUTIL_SSL_USE_OPENSSL)
    int in_init=0;

    while (size > 0 && cnt < timeout * 10) {
        if (SSL_in_init(isck->session) &&
                !SSL_total_renegotiations(isck->session)) {
            in_init = 1;
        } else {
            if (in_init)
                in_init = 0;
        }

        res = CMUTIL_CALL(sock, CheckWriteBuffer, timeout);
        if (res == CMUTIL_SocketOk) {
            rc = SSL_write(isck->session, buf, size);

            switch (SSL_get_error(isck->session, rc))
            {
            case SSL_ERROR_NONE:
                if (rc <= 0) {
                    CMLogError("SSL write failed");
                    return CMUTIL_SocketSendFailed;
                }
                buf += rc;
                size -= rc;
                if (size == 0)
                    return CMUTIL_SocketOk;
                break;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_X509_LOOKUP:
                cnt++;
                usleep(100000);
                break;
            case SSL_ERROR_ZERO_RETURN:
                CMLogError("peer closed connection");
                return CMUTIL_SocketSendFailed;
            default:
                CMLogError("SSL write failed");
                return CMUTIL_SocketSendFailed;
            }
        } else {
            CMLogError("CheckWriteBuffer() failed");
            return res;
        }

    }
#else

    while (size > 0 && cnt < timeout * 10) {
        int handshake_res = 0;

        res = CMCall(sock, CheckWriteBuffer, timeout);
        if (res != CMUTIL_SocketOk)
            return res;

        rc = (int)gnutls_record_send(isck->session, buf, (uint32_t)size);
        switch (rc) {
        case GNUTLS_E_REHANDSHAKE:
            do {
                handshake_res = gnutls_handshake(isck->session);
            } while (handshake_res < 0 &&
                     gnutls_error_is_fatal(handshake_res) == 0);
            if (handshake_res < 0) {
                CMLogError("handshake failed : %s", gnutls_strerror(rc));
                return CMUTIL_SocketSendFailed;
            }
            break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
            cnt++;
            usleep(100000);
            break;
        default:
            if (rc < 0) {
                if (gnutls_error_is_fatal(rc) == 0) {
                    CMLogWarn("Warning : %s", gnutls_strerror(rc));
                } else {
                    CMLogError("send failed : %s", gnutls_strerror(rc));
                    return CMUTIL_SocketSendFailed;
                }
            } else if (rc == 0) {
                CMLogError("peer closed connection");
                return CMUTIL_SocketSendFailed;
            } else {
                buf += rc;
                size -= (uint32_t)rc;
            }
        }
    }
#endif  // !CMUTIL_SSL_USE_OPENSSL
    if (cnt < timeout * 10)
        return CMUTIL_SocketOk;
    else {
        CMLogError("send timed out.");
        return CMUTIL_SocketTimeout;
    }
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SSLSocketWriteSocket(
        const CMUTIL_Socket *sock,
        CMUTIL_Socket *tobesent, pid_t pid, long timeout)
{
    CMUTIL_UNUSED(sock, tobesent, pid, timeout);
    CMLogError("socket passing not allowed in SSL socket");
    return CMUTIL_SocketReceiveFailed;
}

CMUTIL_STATIC void CMUTIL_SSLSocketClose(
        CMUTIL_Socket *socket)
{
    CMUTIL_SSLSocket_Internal *isck = (CMUTIL_SSLSocket_Internal*)socket;
    if (isck) {
        if (isck->session) {
#if defined(CMUTIL_SSL_USE_OPENSSL)
            //SSL_shutdown(isck->session);
            isck->base.sock = SSL_get_fd(isck->session);
            SSL_free(session->session);
#else
            //gnutls_bye(isck->session, GNUTLS_SHUT_WR);
            gnutls_deinit(isck->session);
            if (isck->cred)
                gnutls_certificate_free_credentials(isck->cred);
#endif  // !CMUTIL_SSL_USE_OPENSSL
        }
        CMCall(socket, Close);
    }
}

static CMUTIL_Socket g_cmutil_sslsocket = {
    CMUTIL_SSLSocketRead,
    CMUTIL_SocketWrite,
    CMUTIL_SSLSocketWritePart,
    CMUTIL_SSLSocketCheckReadBuffer,
    CMUTIL_SSLSocketCheckWriteBuffer,
    CMUTIL_SSLSocketReadSocket,
    CMUTIL_SSLSocketWriteSocket,
    CMUTIL_SocketGetRemoteAddr,
    CMUTIL_SSLSocketClose
};

CMUTIL_STATIC CMUTIL_SSLSocket_Internal *CMUTIL_SSLSocketCreate(
        CMUTIL_Mem *memst, CMUTIL_Bool silent)
{
    CMUTIL_SSLSocket_Internal *res = NULL;
    res = memst->Alloc(sizeof(CMUTIL_SSLSocket_Internal));
    memset(res, 0x0, sizeof(CMUTIL_SSLSocket_Internal));
    res->base.sock = INVALID_SOCKET;
    res->base.silent = silent;
    res->base.memst = memst;
    memcpy(res, &g_cmutil_sslsocket, sizeof(CMUTIL_Socket));
    return res;
}

#if defined(CMUTIL_SSL_USE_OPENSSL)
#define SSL_CHECK(x, b)    do { \
    int rval = (x); \
    if (rval <= 0) { \
        if (!silent) { char buf[1024]; \
        ERR_error_string(ERR_peek_last_error(), buf);   \
        CMLogError("%s failed - %s", #x, buf); } \
        goto b; \
    }} while(0)
#else
#define SSL_CHECK(x, b)    do { \
    int rval = (x); \
    if (rval < 0) { \
        if (!silent) CMLogError("%s failed : %s", #x, gnutls_strerror(rval)); \
        goto b; \
    }} while(0)
#endif  // !CMUTIL_SSL_USE_OPENSSL

CMUTIL_Socket *CMUTIL_SSLSocketConnectInternal(
        CMUTIL_Mem *memst,
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout,
        CMUTIL_Bool silent)
{
    CMUTIL_SSLSocket_Internal *res = CMUTIL_SSLSocketCreate(memst, silent);
#if defined(CMUTIL_SSL_USE_OPENSSL)
    static const SSL_METHOD *method = SSLv23_server_method();
    if (ca || (cert && key)) {
        res->sslctx = SSL_CTX_new(method);
        if (res->sslctx == NULL) {
            char buf[1024];
            ERR_error_string(ERR_peek_last_error(), buf);
            CMLogError("Unable to create SSL context : %s", buf);
        }
    }
    if (ca)
        SSL_CHECK(SSL_CTX_use_certificate_chain_file(
                      res->sslctx, ca), CLEANUP);
    if (cert && key) {
        SSL_CHECK(SSL_CTX_use_certificate_file(
                      res->sslctx, cert, SSL_FILETYPE_PEM), CLEANUP);
        SSL_CHECK(SSL_CTX_use_PrivateKey_file(
                      res->sslctx, key, SSL_FILETYPE_PEM), CLEANUP);
        SSL_CHECK(SSL_CTX_check_private_key(res->sslctx), CLEANUP);
    }

    res->session = SSL_new(res->sslctx);
    // TODO: server name setting
#else
    if (ca || (cert && key))
        SSL_CHECK(gnutls_certificate_allocate_credentials(
                      &res->cred), FAILEDPOINT);
    if (ca)
        SSL_CHECK(gnutls_certificate_set_x509_trust_file(
                      res->cred, ca, GNUTLS_X509_FMT_PEM), FAILEDPOINT);
    if (cert && key)
        SSL_CHECK(gnutls_certificate_set_x509_key_file(
                      res->cred, cert, key, GNUTLS_X509_FMT_PEM),
                  FAILEDPOINT);

    SSL_CHECK(gnutls_init(&res->session, GNUTLS_CLIENT), FAILEDPOINT);

    if (servername) {
        SSL_CHECK(gnutls_server_name_set(
                      res->session, GNUTLS_NAME_DNS, servername,
                      strlen(servername)), FAILEDPOINT);
        gnutls_session_set_verify_cert(res->session, servername, 0);
    }

    SSL_CHECK(gnutls_set_default_priority(res->session), FAILEDPOINT);
    if (res->cred) {
        SSL_CHECK(gnutls_credentials_set(
                      res->session, GNUTLS_CRD_CERTIFICATE, res->cred),
                  FAILEDPOINT);
    }
#endif  // !CMUTIL_SSL_USE_OPENSSL
    if (CMUTIL_SocketConnectBase(
                (CMUTIL_Socket_Internal*)res, host, port, timeout, silent)) {
        int ir;
        // handshake
#if defined(CMUTIL_SSL_USE_OPENSSL)
        SSL_set_fd(res->session, res->base.sock);
        SSL_CHECK(SSL_connect(res->session), FAILEPOINT);
#else
        gnutls_transport_set_int(res->session, res->base.sock);
        gnutls_handshake_set_timeout(
                    res->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
        do {
            ir = gnutls_handshake(res->session);
        } while (ir < 0 && gnutls_error_is_fatal(ir) == 0);
        if (ir < 0) {
            if (!silent) {
                if (ir == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {
                    gnutls_datum_t out;
                    gnutls_certificate_type_t type;
                    unsigned status;
                    /* check certificate verification status */
                    type = gnutls_certificate_type_get(res->session);
                    status = gnutls_session_get_verify_cert_status(
                                res->session);
                    if (gnutls_certificate_verification_status_print(
                                status, type, &out, 0) < 0) {
                        CMLogError("server certificate verification failed.");
                    } else {
                        CMLogError("server certificate verification failed: %s",
                                   out.data);
                        gnutls_free(out.data);
                    }
                }
                CMLogError("SSL handshake failed : %s", gnutls_strerror(ir));
            }
            goto FAILEDPOINT;
        }
#endif  // !CMUTIL_SSL_USE_OPENSSL
        return (CMUTIL_Socket*)res;
    } else if (!silent) {
        CMLogError("CMUTIL_SocketConnectBase() failed.");
    }
FAILEDPOINT:
    CMCall((CMUTIL_Socket*)res, Close);
    return NULL;
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_SSLServerSocketAccept(
        const CMUTIL_ServerSocket *server, long timeout)
{
#if !defined(CMUTIL_SSL_USE_OPENSSL)
    int handshake_res;
#endif
    const CMUTIL_SSLServerSocket_Internal *issock =
            (const CMUTIL_SSLServerSocket_Internal*)server;
    CMUTIL_SSLSocket_Internal *res = CMUTIL_SSLSocketCreate(
                issock->base.memst, CMFalse);
    CMUTIL_Bool silent = issock->base.silent;

    if (CMUTIL_ServerSocketAcceptInternal(
                server, (CMUTIL_Socket*)res, timeout)) {
#if defined(CMUTIL_SSL_USE_OPENSSL)
        res->session = SSL_new(issock->sslctx);
        SSL_set_fd(res->session, res->base.sock);
        SSL_CHECK(SSL_accept(res->session), FAILEDPOINT);
#else
        SSL_CHECK(gnutls_init(&(res->session), GNUTLS_SERVER), FAILEDPOINT);
        SSL_CHECK(gnutls_priority_set(
                      res->session, issock->priority_cache), FAILEDPOINT);
        SSL_CHECK(gnutls_credentials_set(
                      res->session, GNUTLS_CRD_CERTIFICATE, issock->cred),
                  FAILEDPOINT);

        gnutls_handshake_set_timeout(
                    res->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
        gnutls_record_set_timeout(res->session, 10000);
        gnutls_session_enable_compatibility_mode(res->session);
        gnutls_transport_set_int(res->session, res->base.sock);

        do {
            handshake_res = gnutls_handshake(res->session);
        } while (handshake_res < 0 &&
                 gnutls_error_is_fatal(handshake_res) == 0);
        SSL_CHECK(handshake_res, FAILEDPOINT);
#endif  // !CMUTIL_SSL_USE_OPENSSL
    }
    return (CMUTIL_Socket*)res;
FAILEDPOINT:
    CMCall((CMUTIL_Socket*)res, Close);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_SSLServerSocketClose(CMUTIL_ServerSocket *ssock)
{
    CMUTIL_SSLServerSocket_Internal *issock =
            (CMUTIL_SSLServerSocket_Internal*)ssock;
    if (issock) {
#if defined(CMUTIL_SSL_USE_OPENSSL)
        if (issock->sslctx)
            SSL_CTX_free(issock->sslctx);
#else
# if GNUTLS_VERSION_NUMBER < 0x030506
        if (issock->dh_params)
            gnutls_dh_params_deinit(issock->dh_params);
# endif
        if (issock->cred)
            gnutls_certificate_free_credentials(issock->cred);
        if (issock->priority_cache)
            gnutls_priority_deinit(issock->priority_cache);
#endif  // !CMUTIL_SSL_USE_OPENSSL
        CMCall(ssock, Close);
    }
}

static CMUTIL_ServerSocket g_cmutil_sslserversocket = {
    CMUTIL_SSLServerSocketAccept,
    CMUTIL_SSLServerSocketClose
};

CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreateInternal(
        CMUTIL_Mem *memst, const char *host, int port, int qcnt,
        const char *cert, const char *key, const char *ca,
        CMUTIL_Bool silent)
{
    CMUTIL_SSLServerSocket_Internal *res =
            memst->Alloc(sizeof(CMUTIL_SSLServerSocket_Internal));
    memset(res, 0x0, sizeof(CMUTIL_SSLServerSocket_Internal));
    res->base.memst = memst;
    res->base.silent = silent;
    memcpy(res, &g_cmutil_sslserversocket, sizeof(CMUTIL_ServerSocket));
    if (CMUTIL_ServerSocketCreateBase(
                (CMUTIL_ServerSocket_Internal*)res, host, port, qcnt)) {
#if defined(CMUTIL_SSL_USE_OPENSSL)
        static const SSL_METHOD *method = SSLv23_server_method();
        res->sslctx = SSL_CTX_new(method);
        if (res->sslctx == NULL && !silent) {
            char buf[1024];
            ERR_error_string(ERR_peek_last_error(), buf);
            CMLogError("Unable to create SSL context : %s", buf);
            goto FAILEDPOINT;
        }
        if (ca)
            SSL_CHECK(SSL_CTX_use_certificate_chain_file(
                          res->sslctx, ca), FAILEDPOINT);
        SSL_CHECK(SSL_CTX_use_certificate_file(
                      res->sslctx, cert, SSL_FILETYPE_PEM), FAILEDPOINT);
        SSL_CHECK(SSL_CTX_use_PrivateKey_file(
                      res->sslctx, key, SSL_FILETYPE_PEM), FAILEDPOINT);

        SSL_CHECK(SSL_CTX_check_private_key(res->sslctx), FAILEDPOINT);
#else
        SSL_CHECK(gnutls_certificate_allocate_credentials(
                      &(res->cred)), FAILEDPOINT);
        // CA file(ex. "/etc/ssl/certs/ca-certificates.crt")
        if (ca)
            SSL_CHECK(gnutls_certificate_set_x509_trust_file(
                          res->cred, ca, GNUTLS_X509_FMT_PEM), FAILEDPOINT);
        // CERT file(ex. "cert.pem"), KEY file(ex. "key.pem")
        SSL_CHECK(gnutls_certificate_set_x509_key_file(
                      res->cred, cert, key,
                      GNUTLS_X509_FMT_PEM), FAILEDPOINT);
        SSL_CHECK(gnutls_priority_init(
                      &(res->priority_cache), NULL, NULL), FAILEDPOINT);
# if GNUTLS_VERSION_NUMBER >= 0x030506
        /* only available since GnuTLS 3.5.6, on previous versions see
         * gnutls_certificate_set_dh_params(). */
        SSL_CHECK(gnutls_certificate_set_known_dh_params(
                    res->cred, GNUTLS_SEC_PARAM_MEDIUM), FAILEDPOINT);
# else
        gnutls_dh_params_init(&res->dh_params);
        gnutls_dh_params_generate2(res->dh_params, 1024);
        gnutls_certificate_set_dh_params(res->cred, res->dh_params);
# endif
#endif
        return (CMUTIL_ServerSocket*)res;
    }
FAILEDPOINT:
    CMCall((CMUTIL_ServerSocket*)res, Close);
    return NULL;
}

#else
CMUTIL_Socket *CMUTIL_SSLSocketConnectInternal(
        CMUTIL_Mem *memst,
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout,
        CMUTIL_Bool silent)
{
    CMUTIL_UNUSED(memst, cert, key, ca, servername,
                  host, port, timeout, silent);
    CMLogError("SSL not enabled. recompile with CMUTIL_SUPPORT_SSL.");
    return NULL;
}

CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreateInternal(
        CMUTIL_Mem *memst, const char *host, int port, int qcnt,
        const char *cert, const char *key, const char *ca,
        CMUTIL_Bool silent)
{
    CMUTIL_UNUSED(memst, host, port, qcnt, cert, key, ca, silent);
    CMLogError("SSL not enabled. recompile with CMUTIL_SUPPORT_SSL.");
    return NULL;
}
#endif  // !CMUTIL_SUPPORT_SSL

CMUTIL_Socket *CMUTIL_SSLSocketConnect(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout)
{
    return CMUTIL_SSLSocketConnectInternal(
                CMUTIL_GetMem(), cert, key, ca, servername,
                host, port, timeout, CMFalse);
}

CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreate(
        const char *cert, const char *key, const char *ca,
        const char *host, int port, int qcnt)
{
    return CMUTIL_SSLServerSocketCreateInternal(
                CMUTIL_GetMem(), host, port, qcnt,
                cert, key, ca, CMFalse);
}
