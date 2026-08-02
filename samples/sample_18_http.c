/*
 * 18 - HTTP client
 *
 * Shows: CMUTIL_HttpClient GET and POST, request headers, keep-alive and the
 *        TLS verification knobs.
 *
 * This sample talks to a real host, so it reports a warning and still exits
 * successfully when the network is unavailable. Pass a different URL prefix
 * as the first argument to point it somewhere else.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.http")

#define DEFAULT_URL "https://example.com"

static void log_response(const char *what, int status, CMUTIL_ByteBuffer *body)
{
    if (body == NULL) {
        CMLogWarn("%s failed - is the host reachable?", what);
        return;
    }

    CMLogInfo("%s -> status %d, %u bytes",
              what, status, (unsigned)CMCall(body, GetSize));

    {
        const char *bytes = (const char*)CMCall(body, GetBytes);
        uint32_t size = (uint32_t)CMCall(body, GetSize);
        if (size > 200) size = 200;
        CMLogInfo("first %u bytes:\n%.*s", (unsigned)size, (int)size, bytes);
    }
}

int main(int argc, char **argv)
{
    const char *url = argc > 1 ? argv[1] : DEFAULT_URL;
    CMUTIL_HttpClient *client;
    CMUTIL_Map *headers;
    CMUTIL_ByteBuffer *body;
    CMUTIL_ByteBuffer *payload;
    int status = 0;

    sample_init();

    SAMPLE_SECTION("creating the client");

    /* The client takes a URL prefix; requests use relative URIs. */
    client = CMUTIL_HttpClientCreate(url);
    if (client == NULL) {
        CMLogError("could not create a client for %s", url);
        return sample_exit(1);
    }

    /* Connections are kept alive by default. */
    CMCall(client, SetKeepAlive, CMFalse);

    /*
     * For HTTPS a fresh client verifies the host name (verify_peer controls
     * whether a *client* certificate is presented, and is off).
     *
     *   CMCall(client, SetSSLCert, NULL, NULL, "/etc/ssl/private-ca.pem");
     *   CMCall(client, SetVerify, CMFalse, CMFalse);
     *
     * One of the two is needed to reach a server with a self-signed
     * certificate; the default rejects it.
     */

    SAMPLE_SECTION("GET");

    body = CMCall(client, Get, NULL, "/", &status, 5000);
    log_response("GET /", status, body);
    if (body) CMCall(body, Destroy);

    SAMPLE_SECTION("GET with request headers");

    headers = CMUTIL_MapCreate();
    CMCall(headers, Put, "Accept", "text/html", NULL);
    CMCall(headers, Put, "User-Agent", "libcmutils-sample/1.0", NULL);

    status = 0;
    body = CMCall(client, Get, headers, "/", &status, 5000);
    log_response("GET / with headers", status, body);
    if (body) CMCall(body, Destroy);

    SAMPLE_SECTION("POST");

    /* Request bodies are byte buffers. */
    payload = CMUTIL_ByteBufferCreateEx(64);
    CMCall(payload, AddBytes, (const uint8_t*)"field=value", 11);

    CMCall(headers, Put, "Content-Type", "application/x-www-form-urlencoded",
           NULL);

    status = 0;
    body = CMCall(client, Post, headers, "/", payload, &status, 5000);
    log_response("POST /", status, body);
    if (body) CMCall(body, Destroy);

    /*
     * Request(method, headers, uri, body, &status, timeout) is the general
     * form behind Get and Post - use it for PUT, DELETE, PATCH and friends.
     */
    SAMPLE_SECTION("arbitrary method");

    status = 0;
    body = CMCall(client, Request, "HEAD", headers, "/", NULL, &status, 5000);
    log_response("HEAD /", status, body);
    if (body) CMCall(body, Destroy);

    CMCall(payload, Destroy);
    CMCall(headers, Destroy);
    CMCall(client, Destroy);

    /* A failed request is not a failed sample: the network may be absent. */
    return sample_exit(0);
}
