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
    const char *v = NULL;
    CMUTIL_HttpMethod_Internal *mi = (CMUTIL_HttpMethod_Internal*)method;
    v = CMCall(data, GetCString);
    CMCall(mi->params, PutString, name, v);
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
    const char *v = NULL;
    CMUTIL_HttpMethod_Internal *mi = (CMUTIL_HttpMethod_Internal*)method;
    v = CMCall(data, GetCString);
    CMCall(mi->headers, PutString, name, v);
    return method;
}
