/*
 * 21 - REST client
 *
 * Shows: CMUTIL_RestClient GET / POST / PUT / DELETE against a JSON API, the
 *        status code behind a failed call, request headers, the timeout, and
 *        the CMUTIL_HttpClient half a REST client inherits.
 *
 * A REST client is a CMUTIL_HttpClient that speaks CMUTIL_Json instead of
 * CMUTIL_ByteBuffer: it serializes the request body, adds the two JSON content
 * negotiation headers when the caller did not, and parses the response.
 *
 * The server runs in this same process on 127.0.0.1:19797 - just enough HTTP
 * to answer the sample - so nothing here depends on the network.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.rest")

#define HOST "127.0.0.1"
#define PORT 19797

static CMBool g_running = CMTrue;

/* The resource the API serves, kept in memory. */
static CMUTIL_JsonObject *g_user = NULL;

/* --------------------------------------------------------------- server */

/** Read one CRLF terminated line. Returns CMFalse at end of stream. */
static CMBool read_line(CMUTIL_Socket *sock, char *buf, size_t len)
{
    size_t i = 0;
    while (i < len - 1) {
        int b = CMCall(sock, ReadByte, 2000);
        if (b < 0) return CMFalse;
        if (b == '\n') break;
        if (b != '\r') buf[i++] = (char)b;
    }
    buf[i] = '\0';
    return CMTrue;
}

static void write_response(
        CMUTIL_Socket *sock, int status, const char *reason,
        CMUTIL_Json *body)
{
    CMUTIL_String *out = CMUTIL_StringCreate();
    CMUTIL_String *payload = CMUTIL_StringCreate();
    const char *bytes;
    size_t size;

    if (body) CMCall(body, ToString, payload, CMFalse);
    size = CMCall(payload, GetSize);

    CMCall(out, AddPrint, "HTTP/1.1 %d %s\r\n", status, reason);
    CMCall(out, AddString, "Content-Type: application/json\r\n");
    CMCall(out, AddPrint, "Content-Length: %u\r\n\r\n", (unsigned)size);
    if (size > 0) {
        bytes = CMCall(payload, GetCString);
        CMCall(out, AddNString, bytes, size);
    }

    bytes = CMCall(out, GetCString);
    size = CMCall(out, GetSize);
    CMCall(sock, Write, bytes, (uint32_t)size, 2000);

    CMCall(payload, Destroy);
    CMCall(out, Destroy);
}

/** Answer one request. Returns CMFalse when the connection is finished. */
static CMBool serve_request(CMUTIL_Socket *sock)
{
    char line[1024];
    char method[16] = "";
    char uri[256] = "";
    long clen = 0;
    CMUTIL_Json *body = NULL;

    if (!read_line(sock, line, sizeof(line)) || line[0] == '\0')
        return CMFalse;
    sscanf(line, "%15s %255s", method, uri);

    /* Headers, up to the blank line. Only Content-Length matters here. */
    while (read_line(sock, line, sizeof(line)) && line[0] != '\0') {
        if (strncasecmp(line, "Content-Length:", 15) == 0)
            clen = strtol(line + 15, NULL, 10);
    }

    if (clen > 0) {
        CMUTIL_ByteBuffer *buf = CMUTIL_ByteBufferCreateEx((size_t)clen);
        if (CMCall(sock, Read, buf, (uint32_t)clen, 2000) == CMSocketOk) {
            CMUTIL_String *str = CMUTIL_StringCreate();
            const char *bytes = (const char*)CMCall(buf, GetBytes);
            size_t size = CMCall(buf, GetSize);
            CMCall(str, AddNString, bytes, size);
            body = CMUTIL_JsonParse(str);
            CMCall(str, Destroy);
        }
        CMCall(buf, Destroy);
    }

    CMLogInfo("server <- %s %s", method, uri);

    if (strcmp(uri, "/users/1") != 0) {
        CMUTIL_JsonObject *err = CMUTIL_JsonObjectCreate();
        CMCall(err, PutString, "error", "no such resource");
        CMCall(err, PutString, "path", uri);
        write_response(sock, 404, "Not Found", (CMUTIL_Json*)err);
        CMUTIL_JsonDestroy(err);
    } else if (strcmp(method, "GET") == 0) {
        write_response(sock, 200, "OK", (CMUTIL_Json*)g_user);
    } else if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        if (body && CMCall(body, GetType) == CMJsonTypeObject) {
            /* Merge the posted members into the stored resource. */
            CMUTIL_JsonObject *in = (CMUTIL_JsonObject*)body;
            CMUTIL_StringArray *keys = CMCall(in, GetKeys);
            uint32_t i;
            for (i = 0; i < (uint32_t)CMCall(keys, GetSize); i++) {
                const char *key = CMCall(keys, GetCString, i);
                CMUTIL_Json *val = CMCall(in, Get, key);
                CMUTIL_Json *copy = CMCall(val, Clone);
                CMUTIL_Json *prev = CMCall(g_user, Remove, key);
                if (prev) CMUTIL_JsonDestroy(prev);
                CMCall(g_user, Put, key, copy);
            }
            CMCall(keys, Destroy);
            /* POST answers with the resource, PUT with nothing. */
            if (strcmp(method, "POST") == 0)
                write_response(sock, 200, "OK", (CMUTIL_Json*)g_user);
            else
                write_response(sock, 204, "No Content", NULL);
        } else {
            CMUTIL_JsonObject *err = CMUTIL_JsonObjectCreate();
            CMCall(err, PutString, "error", "expected a JSON object");
            write_response(sock, 400, "Bad Request", (CMUTIL_Json*)err);
            CMUTIL_JsonDestroy(err);
        }
    } else if (strcmp(method, "DELETE") == 0) {
        CMUTIL_Json *prev = CMCall(g_user, Remove, "role");
        if (prev) CMUTIL_JsonDestroy(prev);
        write_response(sock, 204, "No Content", NULL);
    } else {
        write_response(sock, 405, "Method Not Allowed", NULL);
    }

    if (body) CMUTIL_JsonDestroy(body);
    return CMTrue;
}

static void *server_loop(void *udata)
{
    CMUTIL_ServerSocket *server = (CMUTIL_ServerSocket*)udata;

    while (g_running) {
        CMUTIL_Socket *client = NULL;
        if (CMCall(server, Accept, &client, 500) != CMSocketOk)
            continue;   /* a timeout, or the listener was closed */
        CMCall(client, SetSilent, CMTrue);
        /* The client keeps the connection alive, so keep serving it. */
        while (g_running && serve_request(client))
            ;
        CMCall(client, Close);
    }
    return udata;
}

/* --------------------------------------------------------------- client */

static void show(const char *what, CMUTIL_Json *json)
{
    CMUTIL_String *out;
    const char *text;
    if (json == NULL) {
        CMLogInfo("%s -> (no body)", what);
        return;
    }
    out = CMUTIL_StringCreate();
    CMCall(json, ToString, out, CMFalse);
    text = CMCall(out, GetCString);
    CMLogInfo("%s -> %s", what, text);
    CMCall(out, Destroy);
}

int main(void)
{
    CMUTIL_ServerSocket *server;
    CMUTIL_Thread *server_thread;
    CMUTIL_RestClient *rest;
    CMUTIL_Json *res;
    char url[64];

    sample_init();

    SAMPLE_SECTION("starting the API");

    g_user = CMUTIL_JsonObjectCreate();
    CMCall(g_user, PutLong, "id", 1);
    CMCall(g_user, PutString, "name", "dennis");

    server = CMUTIL_ServerSocketCreate(HOST, PORT, 8, CMFalse);
    if (server == NULL) {
        CMLogError("could not bind %s:%d", HOST, PORT);
        CMUTIL_JsonDestroy(g_user);
        return sample_exit(1);
    }
    CMCall(server, SetSilent, CMTrue);
    server_thread = CMUTIL_ThreadCreate(server_loop, server, "api");
    CMCall(server_thread, Start);

    SAMPLE_SECTION("creating the client");

    snprintf(url, sizeof(url), "http://%s:%d", HOST, PORT);
    rest = CMUTIL_RestClientCreate(url);
    if (rest == NULL) {
        CMLogError("could not create a client for %s", url);
        return sample_exit(1);
    }

    /* The REST methods take no timeout of their own; this is it. */
    CMCall(rest, SetTimeout, 5000L);

    /*
     * The first member of a REST client is a whole CMUTIL_HttpClient, so
     * everything from sample_18 is still available through &rest->base:
     *
     *   CMCall(&rest->base, SetVerify, CMTrue, CMFalse);
     *   CMCall(&rest->base, SetSSLCert, NULL, NULL, "/etc/ssl/my-ca.pem");
     *   CMCall(&rest->base, SetKeepAlive, CMFalse);
     *
     * and so is Destroy - a REST client has no Destroy of its own.
     */

    SAMPLE_SECTION("GET");

    res = CMCall(rest, Get, NULL, "/users/1");
    show("GET /users/1", res);
    CMLogInfo("status %d", CMCall(rest, GetStatus));
    if (res) {
        /* The response is an ordinary CMUTIL_Json - see sample_05. */
        CMUTIL_JsonObject *obj = (CMUTIL_JsonObject*)res;
        const char *name = CMCall(obj, GetCString, "name");
        CMLogInfo("name is \"%s\"", name? name:"(unset)");
        CMUTIL_JsonDestroy(res);
    }

    SAMPLE_SECTION("GET with request headers");

    {
        /* The map is only read, and Accept / Content-Type are filled in
         * only where the caller left them out. */
        CMUTIL_Map *headers = CMUTIL_MapCreate();
        CMCall(headers, Put, "X-Request-Id", "sample-21", NULL);
        CMCall(headers, Put, "User-Agent", "libcmutils-sample/1.0", NULL);

        res = CMCall(rest, Get, headers, "/users/1");
        show("GET /users/1 with headers", res);
        if (res) CMUTIL_JsonDestroy(res);
        CMCall(headers, Destroy);
    }

    SAMPLE_SECTION("POST");

    {
        /* Ownership of the request body stays with the caller. */
        CMUTIL_JsonObject *body = CMUTIL_JsonObjectCreate();
        CMCall(body, PutString, "role", "maintainer");
        CMCall(body, PutBoolean, "active", CMTrue);

        res = CMCall(rest, Post, NULL, "/users/1", (CMUTIL_Json*)body);
        show("POST /users/1", res);
        CMLogInfo("status %d", CMCall(rest, GetStatus));
        if (res) CMUTIL_JsonDestroy(res);
        CMUTIL_JsonDestroy(body);
    }

    SAMPLE_SECTION("PUT");

    {
        /* Put answers CMTrue for any 2xx and discards the response body. */
        CMUTIL_JsonObject *body = CMUTIL_JsonObjectCreate();
        CMBool ok;
        CMCall(body, PutString, "name", "Dennis Park");

        ok = CMCall(rest, Put, NULL, "/users/1", (CMUTIL_Json*)body);
        CMLogInfo("PUT /users/1 -> %s, status %d",
                  ok == CMTrue? "ok":"failed", CMCall(rest, GetStatus));
        CMUTIL_JsonDestroy(body);
    }

    res = CMCall(rest, Get, NULL, "/users/1");
    show("GET /users/1 after the updates", res);
    if (res) CMUTIL_JsonDestroy(res);

    SAMPLE_SECTION("DELETE");

    CMCall(rest, Delete, NULL, "/users/1");
    CMLogInfo("DELETE /users/1 -> status %d", CMCall(rest, GetStatus));

    res = CMCall(rest, Get, NULL, "/users/1");
    show("GET /users/1 after the delete", res);
    if (res) CMUTIL_JsonDestroy(res);

    SAMPLE_SECTION("errors");

    /*
     * A NULL result on its own does not say what went wrong. GetStatus does:
     * it holds the status of the request just made, and stays zero when the
     * request never reached a response at all.
     */
    res = CMCall(rest, Get, NULL, "/users/999");
    show("GET /users/999", res);
    CMLogInfo("status %d - an error body is still JSON, so it comes back",
              CMCall(rest, GetStatus));
    if (res) CMUTIL_JsonDestroy(res);

    {
        CMUTIL_RestClient *nowhere = CMUTIL_RestClientCreate("http://127.0.0.1:1");
        CMCall(nowhere, SetTimeout, 1000L);
        CMLogInfo("the connection errors below are the point of this step");
        res = CMCall(nowhere, Get, NULL, "/");
        CMLogInfo("unreachable host -> status %d (never got a response)",
                  CMCall(nowhere, GetStatus));
        if (res) CMUTIL_JsonDestroy(res);
        CMCall(&nowhere->base, Destroy);
    }

    SAMPLE_SECTION("shutting down");

    CMCall(&rest->base, Destroy);

    g_running = CMFalse;
    CMCall(server, Close);
    CMCall(server_thread, Join);
    CMUTIL_JsonDestroy(g_user);

    return sample_exit(0);
}
