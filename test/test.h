//
// Created by Dennis Park on 25. 12. 13..
//

#ifndef CMUTILS_TEST_H
#define CMUTILS_TEST_H

#include "libcmutils.h"


#define CMUTIL_MEM_TYPE CMMemSystem


#define ASSERT(assert, msg) do {        \
    if (assert) {                       \
        CMLogInfo("%s - passed", msg);  \
    } else {                            \
        CMLogError("%s - failed", msg); \
        goto END_POINT;                 \
    }                                   \
} while(0)


#endif //CMUTILS_TEST_H