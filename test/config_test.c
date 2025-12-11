//
// Created by 박성진 on 25. 12. 11..
//

#include <stdio.h>
#include <string.h>

#include "libcmutils.h"

CMUTIL_LogDefine("test.config")

int main(void) {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_Config *conf = CMUTIL_ConfigCreate();
    CMCall(conf, Set, "config.item.1", "item.value.1");
    CMCall(conf, Set, "config.item.2", "item.value.2");
    CMCall(conf, Set, "config.item.3", "item.value.3");
    CMCall(conf, Set, "config.item.4", "item.value.4");
    CMCall(conf, Set, "config.item.5", "item.value.5");
    CMCall(conf, Save, "test_config.conf");

    CMCall(conf, Destroy); conf = NULL;

    conf = CMUTIL_ConfigLoad("test_config.conf");
    for (int i=0; i<5; i++) {
        char keybuf[128], valbuf[128];
        sprintf(keybuf, "config.item.%d", i+1);
        sprintf(valbuf, "item.value.%d", i+1);
        const char *confval = CMCall(conf, Get, keybuf);
        CMLogInfo("config item %s = %s", keybuf, confval);
        if (strcmp(confval, valbuf) != 0) {
            CMLogError("config value of %s does not match '%' != '%'",
                keybuf, valbuf, confval);
            goto END_POINT;
        }
    }
    CMLogInfo("config test - passed");

    ir = 0;
END_POINT:
    if (conf) CMCall(conf, Destroy);
    CMUTIL_Clear();
    return ir;
}
