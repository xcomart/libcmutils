
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
    const char          *methodstr;
    
} CMUTIL_HttpMethod_Internal;
