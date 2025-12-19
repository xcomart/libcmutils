//
// Created by 박성진 on 25. 12. 16..
//

#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.json")

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);
    CMUTIL_JsonObject *jobj = NULL;
    CMUTIL_Json *jsonAware = NULL;
    CMUTIL_JsonArray *jarr = NULL;
    CMUTIL_String *buf = NULL, *buf2 = NULL;

    jobj = CMUTIL_JsonObjectCreate();
    ASSERT(jobj != NULL, "JsonObjectCreate");

    CMCall(jobj, PutString, "skey", "sval");
    ASSERT(strcmp(CMCall(jobj, GetCString, "skey"), "sval") == 0, "JsonObject::GetCString");

    CMCall(jobj, PutBoolean, "bkey", CMTrue);
    ASSERT(CMCall(jobj, GetBoolean, "bkey"), "JsonObject::GetBoolean");

    CMCall(jobj, PutDouble, "dkey", 0.1234);
    ASSERT(CMCall(jobj, GetDouble, "dkey") - 0.1234 < 0.00001, "JsonObject::GetDouble");

    CMCall(jobj, PutLong, "lkey", 12345);
    ASSERT(CMCall(jobj, GetLong, "lkey") == 12345, "JsonObject::GetLong");

    buf = CMUTIL_StringCreate();
    CMCall((CMUTIL_Json*)jobj, ToString, buf, CMTrue);
    ASSERT(CMCall(buf, GetSize) > 0, "JsonObject::ToString");

    CMLogInfo("JsonObject1: %s", CMCall(buf, GetCString));

    ASSERT(CMCall((CMUTIL_Json*)jobj, GetType) == CMJsonTypeObject, "JsonObject::GetType");
    jsonAware = CMCall((CMUTIL_Json*)jobj, Clone);
    buf2 = CMUTIL_StringCreate();
    CMCall(jsonAware, ToString, buf2, CMTrue);
    CMLogInfo("Cloned json: %s", CMCall(buf2, GetCString));

    ASSERT(CMCall(jsonAware, GetType) == CMJsonTypeObject, "Json::GetType");
    ASSERT(strcmp(CMCall(buf, GetCString), CMCall(buf2, GetCString)) == 0, "JsonObject::Clone");


    jarr = CMUTIL_JsonArrayCreate();
    ASSERT(jarr != NULL, "JsonArrayCreate");

    CMCall(jarr, Add, jsonAware->Clone(jsonAware));
    CMCall(jarr, Add, jsonAware->Clone(jsonAware));
    CMCall(jarr, Add, jsonAware->Clone(jsonAware));
    CMCall(jarr, Add, jsonAware->Clone(jsonAware));
    CMCall(jarr, AddNull);
    CMCall(buf, Clear);
    CMCall((CMUTIL_Json*)jarr, ToString, buf, CMTrue);
    CMLogInfo("Cloned json: %s", CMCall(buf, GetCString));

    const char *jsonstr =
        "{\n"
        "  \"key1\": \"value1\",\n"
        "  \"key2\": \"value2\",\n"
        "  \"key3\": 12345,\n"
        "  \"key4\": 0.1234,\n"
        "  \"key5\": true,\n"
        "  \"key6\": null,\n"
        "  \"arr\": [1, 2, 3, 4, null],\n"
        "  \"obj\": {\"key1\": \"value1\", \"key2\": \"value2\"}\n"
        "}";
    if (buf) CMCall(buf, Destroy); buf = NULL;
    buf = CMUTIL_StringCreate();
    CMCall(buf, AddString, jsonstr);

    if (jobj) CMUTIL_JsonDestroy(jobj); jobj = NULL;
    jobj = (CMUTIL_JsonObject*)CMUTIL_JsonParse(buf);
    ASSERT(jobj != NULL, "JsonParse");
    CMCall(buf, Clear);
    CMCall((CMUTIL_Json*)jobj, ToString, buf, CMTrue);
    CMLogInfo("Parsed json: %s", CMCall(buf, GetCString));
    CMLogInfo("Original json: %s", jsonstr);

    ir = 0;
END_POINT:
    if (buf) CMCall(buf, Destroy);
    if (buf2) CMCall(buf2, Destroy);
    if (jobj) CMUTIL_JsonDestroy(jobj);
    if (jsonAware) CMCall(jsonAware, Destroy);
    if (jarr) CMUTIL_JsonDestroy(jarr);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}