/*
 * 16 - TCP sockets
 *
 * Shows: CMUTIL_ServerSocket accepting into a thread pool, CMUTIL_Socket
 *        reads and writes with per-call timeouts, CMUTIL_SocketAddr, and the
 *        TLS client entry point.
 *
 * Server and clients run inside this one process, so the sample is
 * self-contained: it binds 127.0.0.1:19999.
 *
 * Every socket call returns a CMSocketResult, which makes a timeout a normal
 * outcome (CMSocketTimeout) rather than an error.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.socket")

#define HOST "127.0.0.1"
#define PORT 19999

/* Wire format: a 4 digit zero-padded length, then that many bytes.
 * A length of 0 asks the server to close the connection. */
#define HEADER_SIZE 4

static CMBool g_running = CMTrue;

/* ---------------------------------------------------------------- server */

static void handle_client(void *udata)
{
    CMUTIL_Socket *client = (CMUTIL_Socket*)udata;
    CMUTIL_ByteBuffer *buffer = CMUTIL_ByteBufferCreateEx(1024);
    CMUTIL_String *message = CMUTIL_StringCreate();
    CMUTIL_SocketAddr peer;

    if (CMCall(client, GetRemoteAddr, &peer) == CMSocketOk) {
        char host[128];
        int port = 0;
        CMUTIL_SocketAddrGet(&peer, host, &port);
        CMLogInfo("client connected from %s:%d", host, port);
    }

    while (g_running) {
        CMSocketResult sr;
        int length;

        /* Read blocks until exactly `size` bytes arrive or the timeout hits. */
        CMCall(buffer, Clear);
        sr = CMCall(client, Read, buffer, HEADER_SIZE, 1000);
        if (sr == CMSocketTimeout) continue;
        if (sr != CMSocketOk) break;

        CMCall(message, Clear);
        {
            const char *bytes = (const char*)CMCall(buffer, GetBytes);
            size_t size = CMCall(buffer, GetSize);
            CMCall(message, AddNString, bytes, size);
        }
        length = (int)strtol(CMCall(message, GetCString), NULL, 10);
        if (length <= 0) {
            CMLogInfo("client asked to close");
            break;
        }

        CMCall(buffer, Clear);
        CMCall(message, Clear);
        if (CMCall(client, Read, buffer, (uint32_t)length, 1000) != CMSocketOk)
            break;

        {
            const char *bytes = (const char*)CMCall(buffer, GetBytes);
            size_t size = CMCall(buffer, GetSize);
            CMCall(message, AddNString, bytes, size);
        }
        CMLogInfo("server received: %s", CMCall(message, GetCString));

        /* Echo it back with the same framing. */
        CMCall(message, AddString, " (echoed)");
        {
            size_t size = CMCall(message, GetSize);
            const void *bytes;
            CMCall(message, InsertPrint, 0, "%04d", (int)size);
            bytes = CMCall(message, GetCString);
            size = CMCall(message, GetSize);
            if (CMCall(client, Write, bytes, (uint32_t)size, 1000) != CMSocketOk)
                break;
        }
    }

    CMCall(message, Destroy);
    CMCall(buffer, Destroy);
    /* Sockets are closed, not destroyed. */
    CMCall(client, Close);
}

static void *server_loop(void *udata)
{
    CMUTIL_ServerSocket *server = (CMUTIL_ServerSocket*)udata;
    CMUTIL_ThreadPool *handlers = CMUTIL_ThreadPoolCreate(-1, "handler");

    while (g_running) {
        CMUTIL_Socket *client = NULL;
        CMSocketResult sr = CMCall(server, Accept, &client, 500);

        if (sr == CMSocketOk)
            CMCall(handlers, Execute, handle_client, client);
        else if (sr != CMSocketTimeout)
            break;   /* the listener was closed */
    }

    CMCall(handlers, Wait);
    CMCall(handlers, Destroy);
    return udata;
}

/* ---------------------------------------------------------------- client */

static void client_session(void *udata)
{
    CMUTIL_SocketAddr *addr = (CMUTIL_SocketAddr*)udata;
    CMUTIL_Thread *self = CMUTIL_ThreadSelf();
    const char *name = CMCall(self, GetName);
    CMUTIL_Socket *sock;
    CMUTIL_ByteBuffer *buffer;
    CMUTIL_String *message;
    int i;

    /* CMUTIL_SocketConnect(host, port, timeout, silent) is the other form. */
    sock = CMUTIL_SocketConnectWithAddr(addr, 1000);
    if (sock == NULL) {
        CMLogError("%s could not connect", name);
        return;
    }

    /* Suppresses the library's own error logging on this socket, which is
     * what a busy server wants for expected disconnects. */
    CMCall(sock, SetSilent, CMTrue);

    buffer = CMUTIL_ByteBufferCreateEx(1024);
    message = CMUTIL_StringCreate();

    for (i = 0; i < 3; i++) {
        size_t size;
        const void *bytes;
        int length;

        CMCall(message, Clear);
        CMCall(message, AddPrint, "%s says %d", name, i);
        size = CMCall(message, GetSize);
        CMCall(message, InsertPrint, 0, "%04d", (int)size);

        bytes = CMCall(message, GetCString);
        size = CMCall(message, GetSize);
        if (CMCall(sock, Write, bytes, (uint32_t)size, 1000) != CMSocketOk)
            break;

        CMCall(buffer, Clear);
        if (CMCall(sock, Read, buffer, HEADER_SIZE, 1000) != CMSocketOk)
            break;

        CMCall(message, Clear);
        {
            const char *header = (const char*)CMCall(buffer, GetBytes);
            size_t hsize = CMCall(buffer, GetSize);
            CMCall(message, AddNString, header, hsize);
        }
        length = (int)strtol(CMCall(message, GetCString), NULL, 10);

        CMCall(buffer, Clear);
        CMCall(message, Clear);
        if (CMCall(sock, Read, buffer, (uint32_t)length, 1000) != CMSocketOk)
            break;
        {
            const char *body = (const char*)CMCall(buffer, GetBytes);
            size_t bsize = CMCall(buffer, GetSize);
            CMCall(message, AddNString, body, bsize);
        }
        CMLogInfo("%s got back: %s", name, CMCall(message, GetCString));
    }

    /* Politely ask the handler to stop. */
    CMCall(sock, Write, "0000", 4, 1000);

    CMCall(message, Destroy);
    CMCall(buffer, Destroy);
    CMCall(sock, Close);
}

/* ------------------------------------------------------------------- TLS */

static void sample_tls_note(void)
{
    SAMPLE_SECTION("TLS");

    /*
     * A TLS client connection looks like this - it is not run here because
     * it needs a reachable host:
     *
     *   CMUTIL_Socket *sock = CMUTIL_SSLSocketConnect(
     *           NULL,            // client certificate (PEM), optional
     *           NULL,            // client private key (PEM), optional
     *           NULL,            // CA file; NULL uses the system trust store
     *           "example.com",   // server name: enables verification + SNI
     *           "example.com", 443, 5000);
     *
     * Certificate verification is on by default: with a CA file that file
     * becomes the sole trust anchor and the host name is matched against the
     * certificate; with only a server name the system trust store is used;
     * with neither, verification is explicitly disabled. A private or
     * self-signed certificate therefore has to be opted into.
     *
     * CMUTIL_SSLServerSocketCreate(cert, key, ca, host, port, qcnt) is the
     * server side.
     */
    CMLogInfo("see the comment in %s for the TLS entry points", __FILE__);
}

int main(void)
{
    CMUTIL_ServerSocket *server;
    CMUTIL_Thread *server_thread;
    CMUTIL_ThreadPool *clients;
    CMUTIL_SocketAddr addr;
    int i;

    sample_init();

    SAMPLE_SECTION("starting the server");

    /* (host, port, backlog, silent) */
    server = CMUTIL_ServerSocketCreate(HOST, PORT, 16, CMFalse);
    if (server == NULL) {
        CMLogError("could not bind %s:%d", HOST, PORT);
        return sample_exit(1);
    }
    CMCall(server, SetSilent, CMTrue);

    server_thread = CMUTIL_ThreadCreate(server_loop, server, "server");
    CMCall(server_thread, Start);

    SAMPLE_SECTION("running three clients");

    CMUTIL_SocketAddrSet(&addr, HOST, PORT);
    clients = CMUTIL_ThreadPoolCreate(3, "client");
    for (i = 0; i < 3; i++)
        CMCall(clients, Execute, client_session, &addr);
    CMCall(clients, Wait);
    CMCall(clients, Destroy);

    SAMPLE_SECTION("shutting down");

    g_running = CMFalse;
    /* Closing the listener wakes the accept loop. */
    CMCall(server, Close);
    CMCall(server_thread, Join);

    sample_tls_note();

    return sample_exit(0);
}
