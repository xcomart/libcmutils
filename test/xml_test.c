//
// Created by 박성진 on 25. 12. 23..
//

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.xml")

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_XmlNode *node = NULL;
    CMUTIL_String *str = NULL;
    CMUTIL_Json *json = NULL;

    str = CMUTIL_StringCreateEx(0,
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
        "<!DOCTYPE mapper\n"
        "    PUBLIC \"-//mybatis.org//DTD Mapper 3.0//EN\"\n"
        "    \"../mybatis-3-mapper.dtd\">\n"
        "<xml><a attr=\"a attribute\">1</a><b>2</b></xml>");
    node = CMUTIL_XmlParse(str);
    ASSERT(node != NULL, "CMUTIL_XmlParse");

    if (str) CMCall(str, Destroy); str = NULL;
    str = CMCall(node, ToDocument, CMFalse);

    CMLogInfo("Document: %s", CMCall(str, GetCString));

    json = CMUTIL_XmlToJson(node);

    CMCall(str, Clear);
    json->ToString(json, str, CMTrue);
    CMLogInfo("converted json: %s", CMCall(str, GetCString));

    ir = 0;
END_POINT:
    if (json) CMUTIL_JsonDestroy(json);
    if (str) CMCall(str, Destroy);
    if (node) CMCall(node, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}
