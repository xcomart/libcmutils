
#include "functions.h"

static const char *g_cmutil_http_methods[] = {
    "GET",
    "POST",
    "PUT",
    "HEAD",
    "OPTIONS",
    "DELETE",
    "PATCH",
    "TRACE"
};

typedef struct CMUTIL_HttpMethod_Internal {
    CMUTIL_HttpMethod   base;
    CMHttpMethodType    methodType;
    CMUTIL_ByteBuffer   *rawData;
    CMUTIL_JsonObject   *params;
    CMUTIL_JsonObject   *headers;
} CMUTIL_HttpMethod_Internal;

CMUTIL_HttpMethod *CMUTIL_HttpMethodAddFormData(
        CMUTIL_HttpMethod *method, const char *name, CMUTIL_String *data)
{
    CMUTIL_HttpMethod_Internal *mi = (CMUTIL_HttpMethod_Internal*)method;
    CMCall(mi->params, PutString, name, CMCall(data, GetCString));
    return method;
}

CMUTIL_HttpMethod *CMUTIL_HttpMethodSetRawData(
        CMUTIL_HttpMethod *method, CMUTIL_ByteBuffer *data)
{
    CMUTIL_HttpMethod_Internal *mi = (CMUTIL_HttpMethod_Internal*)method;
    mi->rawData = data;
    return method;
}

CMUTIL_HttpMethod *CMUTIL_HttpMethodSetRequestHeader(
        CMUTIL_HttpMethod *method, const char *name, CMUTIL_String *data)
{
    CMUTIL_HttpMethod_Internal *mi = (CMUTIL_HttpMethod_Internal*)method;
    CMCall(mi->headers, PutString, name, CMCall(data, GetCString));
    return method;
}
