//
// Created by 박성진 on 25. 12. 10..
//

#include "../src/libcmutils.h"

CMUTIL_LogDefine("test.concurrent")

typedef struct {
    int value;
    CMUTIL_Mutex *mutex;
} ThreadParam;

void *thread_func(void *param) {
    ThreadParam *tp = (ThreadParam *)param;
    CMLogInfo("log in thread - id: %u, systemId: "PRINT64U,
        CMUTIL_ThreadSelfId(),
        CMUTIL_ThreadSystemSelfId());
    // wait 1 second
    usleep(1000 * 1000);
    CMCall(tp->mutex, Lock);
    tp->value++;
    CMCall(tp->mutex, Unlock);
    return NULL;
}

void thread_func2(void *param) {
    thread_func(param);
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_ThreadPool *tpool = NULL;
    CMUTIL_Mutex *mtx = CMUTIL_MutexCreate();
    CMLogInfo("mutex creation - passed");
    ThreadParam tp = {0, mtx};
    CMUTIL_Thread *t = CMUTIL_ThreadCreate(&thread_func, &tp, "test1");
    CMLogInfo("thread creation - passed");
    CMCall(t, Start);
    if (tp.value != 0) {
        CMLogError("thread test - failed");
        goto END_POINT;
    }
    CMCall(t, Join); t = NULL;
    if (tp.value != 1) {
        CMLogError("thread test - failed");
        goto END_POINT;
    }
    CMLogInfo("thread test - passed");

    tpool = CMUTIL_ThreadPoolCreate(10);
    for (int i=0; i<10; i++)
        CMCall(tpool, Execute, thread_func2, &tp);

    CMCall(tpool, Wait);

    CMLogInfo("threadpool test end");

    ir = 0;
END_POINT:
    if (tpool) CMCall(tpool, Destroy);
    if (mtx) CMCall(mtx, Destroy);
    if (t) CMCall(t, Join);
    CMUTIL_Clear();
    return ir;
}