//
// Created by Dennis Park on 26. 2. 2..
//

#include <stdio.h>
#include <stdlib.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.http")

int main() {
    int ir = -1;
    CMUTIL_HttpClient *client = NULL;
    CMUTIL_ByteBuffer *buffer = NULL;
    int status = 0;
    CMUTIL_Init(CMMemDebug);
    client = CMUTIL_HttpClientCreate("https://aihouse.synology.me:5001");
    // client = CMUTIL_HttpClientCreate("https://www.naver.com");
    ASSERT(client != NULL, "CMUTIL_HttpClientCreate");

    CMCall(client, SetKeepAlive, CMTrue);

    buffer = CMCall(client, Get, NULL, "/", &status, 5000);
    ASSERT(buffer != NULL, "CMUTIL_HttpClientGet");
    // ASSERT(status == 200, "CMUTIL_HttpClientGet");
    *(CMCall(buffer, GetBytes) + CMCall(buffer, GetSize)) = 0;

    CMLogInfo("HTTP GET / status: %d", status);
    CMLogInfo("HTTP GET / response: %s", CMCall(buffer, GetBytes));

    ir = 0;
END_POINT:
    if (buffer) CMCall(buffer, Destroy);
    if (client) CMCall(client, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}
