//
// Created by 박성진 on 25. 12. 10..
//

#include <stdio.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.concurrent")

typedef struct {
    int value;
    CMUTIL_Mutex *mutex;
} ThreadParam;

void *thread_func(void *param) {
    ThreadParam *tp = (ThreadParam *)param;
    CMUTIL_Thread *t = CMUTIL_ThreadSelf();
    CMLogInfo("log in thread - id: %u, systemId: %"PRIu64", name: %s",
        CMUTIL_ThreadSelfId(),
        CMUTIL_ThreadSystemSelfId(),
        CMCall(t, GetName));
    // wait 100 milliseconds
    usleep(100000);
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
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    CMUTIL_ThreadPool *tpool = NULL;
    CMUTIL_Mutex *mtx = NULL;
    ThreadParam tp;
    CMUTIL_Thread *t = NULL;

    mtx = CMUTIL_MutexCreate();
    ASSERT(mtx != NULL, "CMUTIL_MutexCreate");

    tp.value = 0;
    tp.mutex = mtx;

    t = CMUTIL_ThreadCreate(&thread_func, &tp, NULL);
    ASSERT(t != NULL, "CMUTIL_ThreadCreate");
    CMCall(t, Start);
    ASSERT(tp.value == 0, "thread started");

    CMCall(t, Join); t = NULL;
    ASSERT(tp.value == 1, "thread joined");

    tpool = CMUTIL_ThreadPoolCreate(-1, NULL);
    ASSERT(tpool != NULL, "CMUTIL_ThreadPoolCreate with dynamic size");
    for (int i=0; i<10; i++)
        CMCall(tpool, Execute, thread_func2, &tp);
    ASSERT(tp.value == 1, "after 10 asynchronous calls");

    CMCall(tpool, Wait);
    ASSERT(tp.value == 11, "Threadpool Wait");
    CMLogInfo("ThreadPool operations contains CMUTIL_Thread, "
              "CMUTIL_Mutex, CMUTIL_Cond and CMUTIL_Semaphore tests. "
              "So if this log shown without any error, "
              "all concurrent objects are working properly.");

    ir = 0;
END_POINT:
    if (tpool) CMCall(tpool, Destroy);
    if (mtx) CMCall(mtx, Destroy);
    if (t) CMCall(t, Join);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}