//
// Created by 박성진 on 25. 12. 19.
//

#include <stdio.h>
#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.map")

int main() {
    int ir = -1;
    CMUTIL_ConfLogger *logger = NULL;
    CMUTIL_LogAppender *apndr = NULL;
    CMUTIL_Map *map = NULL;

    CMUTIL_Init(CMUTIL_MEM_TYPE);

    const char *pattern = "%d{%F %T.%Q} %P-[%-10t] (%-15F:%04L) [%05p] %c : %m%n%ex";

    CMUTIL_LogSystem *lsys = CMUTIL_LogSystemCreate();
    ASSERT(lsys != NULL, "CMUTIL_LogSystemCreate");

    logger = CMCall(lsys, CreateLogger, NULL, CMLogLevel_Trace, CMTrue);
    ASSERT(logger != NULL, "LogSystem::CreateLogger");

    apndr = CMUTIL_LogConsoleAppenderCreate("Console", pattern, CMFalse);
    ASSERT(apndr != NULL, "LogConsoleAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Trace); apndr = NULL;

    CMUTIL_LogSystemSet(lsys); lsys = NULL;


    map = CMUTIL_MapCreate();

    ASSERT(map != NULL, "CMUTIL_MapCreate");

    CMCall(map, Put, "key1", "value1", NULL);
    CMCall(map, Put, "key2", "value2", NULL);
    ASSERT(strcmp(CMCall(map, Get, "key1"), "value1") == 0, "CMUTIL_Map Put/Get");

    ASSERT(strcmp(CMCall(map, Remove, "key2"), "value2") == 0, "CMUTIL_Map Remove");

    CMCall(map, Destroy); map = NULL;

    map = CMUTIL_MapCreateEx(20, CMTrue, CMUTIL_GetMem()->Free);
    ASSERT(map != NULL, "CMUTIL_MapCreateEx");

    for (int i = 0; i < 100; i++) {
        char kbuf[20];
        char vbuf[20];
        sprintf(kbuf, "key%d", i);
        sprintf(vbuf, "value%d", i);
        CMCall(map, Put, kbuf, CMStrdup(vbuf), NULL);
    }

    ASSERT(strcmp(CMCall(map, Get, "KEY1"), "value1") == 0 && strcmp(CMCall(map, Get, "key1"), "value1") == 0, "CMUTIL_Map Put/Get case insensitive");


    ir = 0;
END_POINT:
    if (apndr) CMCall(apndr, Destroy);
    if (lsys) CMCall(lsys, Destroy);
    if (map) CMCall(map, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}