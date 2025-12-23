//
// Created by 박성진 on 25. 12. 23..
//

#include <stdio.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.pool")

typedef struct PoolTestCtx {
    int value;
    int count;
} PoolTestCtx;

void *pool_create(void *udata) {
    PoolTestCtx *ctx = (PoolTestCtx *)udata;
    char buf[20];
    sprintf(buf, "value:%d", ctx->value++);
    ctx->count++;
    return CMStrdup(buf);
}

void pool_destroy(void *obj, void *udata) {
    PoolTestCtx *ctx = (PoolTestCtx *)udata;
    ctx->count--;
    CMFree(obj);
}

CMBool pool_test(void *obj, void *udata) {
    CMUTIL_UNUSED(udata);
    CMLogInfo("test obj:%s", obj);
    return CMTrue;
}

void pool_test_proc(void *udata) {
    CMUTIL_Pool *pool = (CMUTIL_Pool *)udata;
    for (int i = 0; i < 10; i++) {
        char *data = CMCall(pool, CheckOut, 1000);
        CMLogInfo("%dth checkouted :%s", i+1, data);
        CMCall(pool, Release, data);
    }
}


int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_Pool *pool = NULL;
    CMUTIL_ThreadPool *tpool = NULL;
    PoolTestCtx ctx = {0, 0};


    pool = CMUTIL_PoolCreate(
        5, 10,
        pool_create,
        pool_destroy,
        pool_test,
        1*1000, CMFalse, &ctx, NULL);
    ASSERT(pool != NULL, "CMUTIL_PoolCreate");

    tpool = CMUTIL_ThreadPoolCreate(-1, "PoolTest");
    for (int i = 0; i < 10; i++) {
        CMCall(tpool, Execute, pool_test_proc, pool);
    }

    CMCall(tpool, Wait);

    usleep(1000000);


    ir = 0;
END_POINT:
    if (tpool) CMCall(tpool, Destroy);
    if (pool) CMCall(pool, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}