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
    CMUTIL_Mem_st		*memst;
} CMUTIL_Socket_Internal;

CMUTIL_STATIC CMUTIL_Socket_Internal *CMUTIL_SocketCreate(
        CMUTIL_Mem_st *memst, CMUTIL_Bool silent);

# define CMUTIL_RBUF_LEN    1024
CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketRead(
        CMUTIL_Socket *sock, CMUTIL_String *buffer, int size, long timeout)
{
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
    int width, rc, rsize;
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
        int toberead = size - rsize;
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

        CMUTIL_CALL(buffer, AddNString, buf, rc);
        rsize += rc;
    }
    return CMUTIL_SocketOk;
}

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_SocketReadSocket(
        CMUTIL_Socket *sock, long timeout, CMUTIL_SocketResult *rval)
{
    CMUTIL_Socket_Internal *res = NULL;
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
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
    int bufsiz = 5;
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
    *rval = CMUTIL_CALL(sock, CheckReadBuffer, timeout);

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
        CMUTIL_Socket *sock, CMUTIL_String *data, long timeout)
{
    int length = CMUTIL_CALL(data, GetSize);
    return CMUTIL_CALL(sock, WritePart, data, 0, length, timeout);
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketWritePart(
        CMUTIL_Socket *sock, CMUTIL_String *data,
        int offset, int length, long timeout)
{
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
    int width, rc, size = length;
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
        rc = (int)send(isock->sock, CMUTIL_CALL(data, GetCString) + offset,
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
        size -= rc;
    }

    return CMUTIL_SocketOk;
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketWriteSocket(
        CMUTIL_Socket *sock, CMUTIL_Socket *tobesent, pid_t pid, long timeout)
{
    // TODO:
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
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
    res = CMUTIL_CALL(sock, CheckWriteBuffer, timeout);
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
        CMUTIL_Socket *sock, long timeout, CMUTIL_Bool isread)
{
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
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
        CMUTIL_Socket *sock, long timeout)
{
    return CMUTIL_SocketCheckBuffer(sock, timeout, CMUTIL_True);
}

CMUTIL_STATIC CMUTIL_SocketResult CMUTIL_SocketCheckWriteBuffer(
        CMUTIL_Socket *sock, long timeout)
{
    return CMUTIL_SocketCheckBuffer(sock, timeout, CMUTIL_False);
}

CMUTIL_STATIC void CMUTIL_SocketGetRemoteAddr(
        CMUTIL_Socket *sock, char *hostbuf, int *port)
{
    CMUTIL_Socket_Internal *isock = (CMUTIL_Socket_Internal*)sock;
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
    unsigned long addr;
    struct sockaddr_in *serv = &(is->peer);
    SOCKET s;
    int rc;
    unsigned long arg;
    int iarg;
    int width;
    fd_set wfds;
    struct timeval tv;

CONNECT_RETRY:

    memset(serv, 0x0, sizeof(struct sockaddr_in));
    serv->sin_family = AF_INET;
    serv->sin_port = htons((unsigned short) port);
    addr = (unsigned long)
        ((unsigned long) ip[0] << 24L) | ((unsigned long) ip[1] << 16L) |
        ((unsigned long) ip[2] <<  8L) | ((unsigned long) ip[3]);
    serv->sin_addr.s_addr = htonl(addr);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        if (!silent)
            CMLogError("cannot create socket.(%d:%s)", errno, strerror(errno));
        return CMUTIL_False;
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
        return CMUTIL_False;
    }
#else
    arg = 1;
    rc = ioctlsocket(s, FIONBIO, &arg);
    if (rc < 0) {
        if (!silent)
            CMLogError("ioctl failed.(%d:%s)", errno, strerror(errno));
        closesocket(s);
        return CMUTIL_False;
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
                return CMUTIL_True;
            } else if (rc == 0) {
                if (!silent)
                    CMLogError("connent timeout.");
                closesocket(s);
                // connect timed out
                return CMUTIL_False;
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
        return CMUTIL_False;
    }

    is->sock = s;
    return CMUTIL_True;
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
        CMUTIL_Mem_st *memst, CMUTIL_Bool silent)
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

CMUTIL_Socket *CMUTIL_SocketConnectInternal(
        CMUTIL_Mem_st *memst, const char *host, int port, long timeout,
        CMUTIL_Bool silent)
{
    unsigned char ip[4];
    unsigned int in[4];
    unsigned rc, i;

    CMUTIL_Socket_Internal *res = CMUTIL_SocketCreate(memst, silent);

    rc = sscanf(host, "%u.%u.%u.%u", &(in[0]), &(in[1]), &(in[2]), &(in[3]));
    if (rc == 4) {
        for (i=0 ; i<4 ; i++) {
            if (in[i] > 255) goto FAILED;
            ip[i] = (unsigned char)in[i];
        }
        if (!CMUTIL_SocketConnectByIP(ip, port, res, timeout, 1, silent)) {
            goto FAILED;
        }

    } else {
        char buf[2048];

        struct hostent hp;

        if (gethostbyname_r(host, &hp, buf, sizeof(buf), NULL, NULL) == 0)
            goto FAILED;

        if ((short) hp.h_addrtype != AF_INET)
            goto FAILED;

        for (i=0 ; hp.h_addr_list[i] != NULL ; i++)
        {
            memcpy(ip, hp.h_addr_list[i], 4);

            if (CMUTIL_SocketConnectByIP(ip, port, res, timeout, 0, silent))
                break;
        }

        if (hp.h_addr_list[i] == NULL)
            goto FAILED;
#if defined(MSWIN)
        CMUTIL_UNUSED(buf);
#endif
    }

    return (CMUTIL_Socket*)res;
FAILED:
    CMUTIL_CALL((CMUTIL_Socket*)res, Close);
    return NULL;
}

CMUTIL_Socket *CMUTIL_SocketConnect(
        const char *host, int port, long timeout)
{
    return CMUTIL_SocketConnectInternal(
                __CMUTIL_Mem, host, port, timeout, CMUTIL_False);
}

typedef struct CMUTIL_ServerSocket_Internal {
    CMUTIL_ServerSocket	base;
    SOCKET				ssock;
    CMUTIL_Bool			silent;
    CMUTIL_Mem_st		*memst;
} CMUTIL_ServerSocket_Internal;

CMUTIL_STATIC CMUTIL_Socket *CMUTIL_ServerSocketAccept(
        CMUTIL_ServerSocket *ssock, long timeout)
{
    CMUTIL_ServerSocket_Internal *issock = (CMUTIL_ServerSocket_Internal*)ssock;
    int width, rc;
    fd_set rfds;
    socklen_t len;
    struct timeval tv;
    CMUTIL_Socket_Internal *res = CMUTIL_SocketCreate(
                issock->memst, issock->silent);

    /*
    Kernel object handles are process specific.
    That is, a process must either create the object
    or open an existing object to obtain a kernel object handle.
    The per-process limit on kernel handles is 2^24.
    So SOCKET can be casted to 32bit integer in 64bit windows.
    */
    width = (int)(issock->ssock + 1);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(issock->ssock, &rfds);

    rc = select(width, &rfds, NULL, NULL, &tv);
    if (rc <= 0) {
        if (!issock->silent)
            CMLogErrorS("select failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    len = sizeof(struct sockaddr_in);
    res->sock = accept(issock->ssock, (struct sockaddr*)&res->peer, &len);
    if (res->sock == INVALID_SOCKET) {
        if (!issock->silent)
            CMLogErrorS("accept failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    return (CMUTIL_Socket*)res;
FAILED:
    CMUTIL_CALL((CMUTIL_Socket*)res, Close);
    return NULL;
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

CMUTIL_ServerSocket *CMUTIL_ServerSocketCreateInternal(
        CMUTIL_Mem_st *memst, const char *host, int port, int qcnt,
        CMUTIL_Bool silent)
{
    struct sockaddr_in addr;
    unsigned short s_port = (unsigned short)port;
    SOCKET sock = INVALID_SOCKET;
    CMUTIL_ServerSocket_Internal *res = NULL;
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
        if (!silent)
            CMLogErrorS("cannot create socket", errno, strerror(errno));
        goto FAILED;
    }

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));
    if (rc == SOCKET_ERROR) {
        if (!silent)
            CMLogErrorS("setsockopt failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

#if defined(SUNOS)
    rc = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    if (rc < 0) {
        if (!silent)
            CMLogErrorS("fnctl failed.(%d:%s)", errno, strerror(errno));
        closesocket(sock);
        return NULL;
    }
#else
    arg = 1;
    rc = ioctlsocket(sock, FIONBIO, &arg);
    if (rc < 0) {
        if (!silent)
            CMLogErrorS("ioctl failed.(%d:%s)", errno, strerror(errno));
        closesocket(sock);
        return NULL;
    }
#endif

    rc = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    if (rc == SOCKET_ERROR) {
        if (!silent)
            CMLogErrorS("bind failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    rc = listen(sock, qcnt);
    if (rc == SOCKET_ERROR) {
        if (!silent)
            CMLogErrorS("listen failed.(%d:%s)", errno, strerror(errno));
        goto FAILED;
    }

    res = memst->Alloc(sizeof(CMUTIL_ServerSocket_Internal));
    memset(res, 0x0, sizeof(CMUTIL_ServerSocket_Internal));
    res->ssock = sock;
    res->memst = memst;
    res->silent = silent;

    memcpy(res, &g_cmutil_serversocket, sizeof(CMUTIL_ServerSocket));
    return (CMUTIL_ServerSocket*)res;

FAILED:
    if (sock != INVALID_SOCKET)
        closesocket(sock);
    if (res)
        CMUTIL_CALL((CMUTIL_ServerSocket*)res, Close);
    return NULL;
}

CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
        const char *host, int port, int qcnt)
{
    return CMUTIL_ServerSocketCreateInternal(
                __CMUTIL_Mem, host, port, qcnt, CMUTIL_False);
}
