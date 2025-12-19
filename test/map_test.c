//
// Created by 박성진 on 25. 12. 19.
//

#include <string.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.map")

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_Map *map = CMUTIL_MapCreate();

    ASSERT(map != NULL, "CMUTIL_MapCreate");

    CMCall(map, Put, "key1", "value1");
    CMCall(map, Put, "key2", "value2");
    ASSERT(strcmp(CMCall(map, Get, "key1"), "value1") == 0, "CMUTIL_Map Put/Get");

    ASSERT(strcmp(CMCall(map, Remove, "key2"), "value2") == 0, "CMUTIL_Map Remove");

    CMCall(map, Destroy); map = NULL;

    map = CMUTIL_MapCreateEx(20, CMTrue, CMUTIL_GetMem()->Free);
    ASSERT(map != NULL, "CMUTIL_MapCreateEx");

    CMCall(map, Put, "key1", CMStrdup("value1"));
    CMCall(map, Put, "key2", CMStrdup("value2"));
    ASSERT(strcmp(CMCall(map, Get, "KEY1"), "value1") == 0 && strcmp(CMCall(map, Get, "key1"), "value1") == 0, "CMUTIL_Map Put/Get case insensitive");


    ir = 0;
END_POINT:
    if (map) CMCall(map, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}