//
// Created by 박성진 on 25. 12. 11..
//

#include <stdio.h>
#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.config")

int main(void) {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_Config *conf = CMUTIL_ConfigCreate();
    CMUTIL_File *file = NULL;
    ASSERT(conf != NULL, "CMUTIL_ConfigCreate");

    CMCall(conf, Set, "config.item.1", "1");
    ASSERT(strcmp(CMCall(conf, Get, "config.item.1"), "1") == 0,
        "CMUTIL_Config Set/Get string");
    CMCall(conf, SetLong, "config.item.2", 2L);
    ASSERT(CMCall(conf, GetLong, "config.item.2") == 2,
        "CMUTIL_Config Set/Get long");
    CMCall(conf, SetBoolean, "config.item.3", CMTrue);
    ASSERT(CMCall(conf, GetBoolean, "config.item.3") == CMTrue,
        "CMUTIL_Config Set/Get boolean");
    CMCall(conf, SetDouble, "config.item.4", 1.234);
    ASSERT(CMCall(conf, GetDouble, "config.item.4") - 1.234 < 0.00001,
        "CMUTIL_Config Set/Get double");
    CMCall(conf, Save, "test_config.conf");
    file = CMUTIL_FileCreate("test_config.conf");
    ASSERT(CMCall(file, IsExists), "config file saved");
    CMUTIL_String *content = CMCall(file, GetContents);
    CMLogInfo("config file contents:\n%s", CMCall(content, GetCString));
    CMCall(file, Destroy); file = NULL;
    CMCall(content, Destroy); content = NULL;
    CMCall(conf, Destroy); conf = NULL;

    conf = CMUTIL_ConfigLoad("test_config.conf");
    ASSERT(conf != NULL, "CMUTIL_ConfigLoad");
    ASSERT(strcmp(CMCall(conf, Get, "config.item.1"), "1") == 0,
        "CMUTIL_ConfigLoad string value");
    ASSERT(CMCall(conf, GetLong, "config.item.2") == 2,
        "CMUTIL_ConfigLoad long value");
    ASSERT(CMCall(conf, GetBoolean, "config.item.3") == CMTrue,
        "CMUTIL_ConfigLoad boolean value");
    ASSERT(CMCall(conf, GetDouble, "config.item.4") - 1.234 < 0.00001,
        "CMUTIL_ConfigLoad double value");

    ir = 0;
END_POINT:
    if (file) CMCall(file, Destroy);
    if (conf) CMCall(conf, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}
