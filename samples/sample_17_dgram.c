/*
 * 17 - UDP datagrams
 *
 * Shows: CMUTIL_DGramSocket in both styles - connectionless (SendTo /
 *        RecvFrom) and connected (Connect / Send / Recv) - plus address
 *        handling with CMUTIL_SocketAddr.
 *
 * An echo server thread binds 127.0.0.1:19898 and prefixes every payload it
 * receives with "echo-".
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.dgram")

#define HOST "127.0.0.1"
#define PORT 19898

static CMBool g_running = CMTrue;

static void *echo_server(void *udata)
{
    CMUTIL_DGramSocket *sock = (CMUTIL_DGramSocket*)udata;
    CMUTIL_ByteBuffer *buffer = CMUTIL_ByteBufferCreateEx(2048);

    while (g_running) {
        CMUTIL_SocketAddr peer;
        CMSocketResult sr;

        CMCall(buffer, Clear);
        sr = CMCall(sock, RecvFrom, buffer, &peer, 500);
        if (sr == CMSocketTimeout) continue;
        if (sr != CMSocketOk) break;

        {
            char host[128];
            int port = 0;
            const char *bytes = (const char*)CMCall(buffer, GetBytes);
            uint32_t size = (uint32_t)CMCall(buffer, GetSize);
            CMUTIL_SocketAddrGet(&peer, host, &port);
            CMLogInfo("server got %u bytes from %s:%d: %.*s",
                      (unsigned)size, host, port, (int)size, bytes);
        }

        CMCall(buffer, InsertBytesAt, 0, (const uint8_t*)"echo-", 5);
        if (CMCall(sock, SendTo, buffer, &peer, 500) != CMSocketOk) {
            CMLogError("server could not answer");
            break;
        }
    }

    CMCall(buffer, Destroy);
    return udata;
}

static void log_payload(const char *label, CMUTIL_ByteBuffer *buffer)
{
    const char *bytes = (const char*)CMCall(buffer, GetBytes);
    uint32_t size = (uint32_t)CMCall(buffer, GetSize);
    CMLogInfo("%s: %.*s", label, (int)size, bytes);
}

static void sample_connectionless(CMUTIL_SocketAddr *server_addr)
{
    CMUTIL_DGramSocket *sock;
    CMUTIL_ByteBuffer *buffer;
    CMUTIL_SocketAddr from;
    const char *text = "connectionless hello";

    SAMPLE_SECTION("connectionless - SendTo / RecvFrom");

    /* CMUTIL_DGramSocketCreateBind(addr) creates and binds in one step.
     * Careful: a NULL address there means "do not bind at all", while
     * Bind(sock, NULL) binds an ephemeral port. */
    sock = CMUTIL_DGramSocketCreate();
    buffer = CMUTIL_ByteBufferCreateEx(2048);

    CMCall(buffer, AddBytes, (const uint8_t*)text, (uint32_t)strlen(text));
    if (CMCall(sock, SendTo, buffer, server_addr, 1000) != CMSocketOk) {
        CMLogError("send failed");
    } else {
        CMCall(buffer, Clear);
        if (CMCall(sock, RecvFrom, buffer, &from, 1000) == CMSocketOk) {
            char host[128];
            int port = 0;
            CMUTIL_SocketAddrGet(&from, host, &port);
            log_payload("client received", buffer);
            CMLogInfo("answer came from %s:%d", host, port);
        } else {
            CMLogWarn("no answer within the timeout");
        }
    }

    CMCall(buffer, Destroy);
    CMCall(sock, Close);
}

static void sample_connected(CMUTIL_SocketAddr *server_addr)
{
    CMUTIL_DGramSocket *sock;
    CMUTIL_ByteBuffer *buffer;
    CMUTIL_SocketAddr local;
    CMUTIL_SocketAddr remote;
    char host[128];
    int port = 0;
    const char *text = "connected hello";

    SAMPLE_SECTION("connected - Send / Recv");

    sock = CMUTIL_DGramSocketCreate();

    /*
     * Bind first even though connecting alone would work: GetLocalAddr
     * reports the address recorded at bind time, so an unbound socket has
     * nothing to report. A NULL address binds an ephemeral port.
     */
    CMCall(sock, Bind, NULL);

    if (CMCall(sock, Connect, server_addr) != CMSocketOk) {
        CMLogError("connect failed");
        CMCall(sock, Close);
        return;
    }

    CMLogInfo("IsConnected: %s", CMCall(sock, IsConnected) ? "yes" : "no");

    if (CMCall(sock, GetLocalAddr, &local) == CMSocketOk) {
        CMUTIL_SocketAddrGet(&local, host, &port);
        CMLogInfo("local  %s:%d", host, port);
    }
    if (CMCall(sock, GetRemoteAddr, &remote) == CMSocketOk) {
        CMUTIL_SocketAddrGet(&remote, host, &port);
        CMLogInfo("remote %s:%d", host, port);
    }

    buffer = CMUTIL_ByteBufferCreateEx(2048);
    CMCall(buffer, AddBytes, (const uint8_t*)text, (uint32_t)strlen(text));

    if (CMCall(sock, Send, buffer, 1000) == CMSocketOk) {
        CMCall(buffer, Clear);
        if (CMCall(sock, Recv, buffer, 1000) == CMSocketOk)
            log_payload("client received", buffer);
        else
            CMLogWarn("no answer within the timeout");
    }

    /* A connected datagram socket can go back to connectionless use. */
    CMCall(sock, Disconnect);
    CMLogInfo("after Disconnect, IsConnected: %s",
              CMCall(sock, IsConnected) ? "yes" : "no");

    CMCall(buffer, Destroy);
    CMCall(sock, Close);
}

int main(void)
{
    CMUTIL_DGramSocket *server;
    CMUTIL_Thread *server_thread;
    CMUTIL_SocketAddr server_addr;

    sample_init();

    CMUTIL_SocketAddrSet(&server_addr, HOST, PORT);

    server = CMUTIL_DGramSocketCreate();
    if (CMCall(server, Bind, &server_addr) != CMSocketOk) {
        CMLogError("could not bind %s:%d", HOST, PORT);
        CMCall(server, Close);
        return sample_exit(1);
    }

    server_thread = CMUTIL_ThreadCreate(echo_server, server, "dgram-server");
    CMCall(server_thread, Start);

    sample_connectionless(&server_addr);
    sample_connected(&server_addr);

    g_running = CMFalse;
    CMCall(server_thread, Join);
    CMCall(server, Close);

    return sample_exit(0);
}
