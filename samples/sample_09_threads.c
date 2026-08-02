/*
 * 09 - Threads and synchronization
 *
 * Shows: CMUTIL_Thread, CMUTIL_Mutex and the CMSync block, CMUTIL_Cond,
 *        CMUTIL_Semaphore and CMUTIL_RWLock - one API over pthreads and
 *        Win32 threads.
 *
 * A thread object is released by Join, never by Destroy, and it has to be
 * joined even if it was never started.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.threads")

typedef struct Counter {
    int value;
    CMUTIL_Mutex *mutex;
} Counter;

static void *counter_worker(void *param)
{
    Counter *counter = (Counter*)param;
    CMUTIL_Thread *self = CMUTIL_ThreadSelf();
    int i;

    CMLogInfo("worker '%s' running (internal id %u, system id %" PRIu64 ")",
              CMCall(self, GetName),
              CMUTIL_ThreadSelfId(),
              CMUTIL_ThreadSystemSelfId());

    for (i = 0; i < 1000; i++) {
        CMCall(counter->mutex, Lock);
        counter->value++;
        CMCall(counter->mutex, Unlock);
    }

    /* Whatever is returned here comes back out of Join. */
    return param;
}

static void sample_threads(void)
{
    Counter counter;
    CMUTIL_Thread *a;
    CMUTIL_Thread *b;

    SAMPLE_SECTION("threads and a mutex");

    counter.value = 0;
    counter.mutex = CMUTIL_MutexCreate();   /* recursive */

    a = CMUTIL_ThreadCreate(counter_worker, &counter, "worker-a");
    b = CMUTIL_ThreadCreate(counter_worker, &counter, "worker-b");

    CMCall(a, Start);
    CMCall(b, Start);
    CMLogInfo("worker-a running: %s",
              CMCall(a, IsRunning) ? "yes" : "no");

    /* Join waits for the thread and frees the thread object. */
    CMCall(a, Join);
    CMCall(b, Join);

    CMLogInfo("counter = %d (expected 2000)", counter.value);

    /*
     * CMSync runs a block under the lock. Never return, break, continue or
     * goto out of it - the unlock would be skipped.
     */
    CMSync(counter.mutex, {
        counter.value = 0;
        CMLogInfo("counter reset inside CMSync");
    });

    if (CMCall(counter.mutex, TryLock)) {
        CMLogInfo("TryLock succeeded on an uncontended mutex");
        CMCall(counter.mutex, Unlock);
    }

    CMCall(counter.mutex, Destroy);
}

typedef struct Signal {
    CMUTIL_Cond *ready;
    CMUTIL_Semaphore *slots;
} Signal;

static void *cond_worker(void *param)
{
    Signal *sig = (Signal*)param;

    /* Acquire returns CMFalse when the timeout elapses. */
    if (CMCall(sig->slots, Acquire, 2000) == CMFalse) {
        CMLogError("could not acquire a semaphore slot");
        return NULL;
    }
    CMLogInfo("worker acquired a slot, signalling the main thread");

    CMCall(sig->ready, Set);
    CMCall(sig->slots, Release);
    return param;
}

static void sample_cond_and_semaphore(void)
{
    Signal sig;
    CMUTIL_Thread *worker;

    SAMPLE_SECTION("condition and semaphore");

    /* CMTrue = manual reset: stays set until Reset is called. */
    sig.ready = CMUTIL_CondCreate(CMTrue);
    /* Two permits. */
    sig.slots = CMUTIL_SemaphoreCreate(2);

    worker = CMUTIL_ThreadCreate(cond_worker, &sig, "signaller");
    CMCall(worker, Start);

    if (CMCall(sig.ready, TimedWait, 2000))
        CMLogInfo("main thread saw the condition");
    else
        CMLogWarn("timed out waiting for the condition");

    CMCall(sig.ready, Reset);
    CMCall(worker, Join);

    CMCall(sig.slots, Destroy);
    CMCall(sig.ready, Destroy);
}

typedef struct Shared {
    CMUTIL_RWLock *lock;
    int generation;
} Shared;

static void *reader(void *param)
{
    Shared *shared = (Shared*)param;
    int i;

    for (i = 0; i < 3; i++) {
        CMCall(shared->lock, ReadLock);
        CMLogInfo("reader sees generation %d", shared->generation);
        CMCall(shared->lock, ReadUnlock);
        usleep(10000);
    }
    return param;
}

static void sample_rwlock(void)
{
    Shared shared;
    CMUTIL_Thread *r1;
    CMUTIL_Thread *r2;
    int i;

    SAMPLE_SECTION("reader/writer lock");

    shared.lock = CMUTIL_RWLockCreate();
    shared.generation = 0;

    r1 = CMUTIL_ThreadCreate(reader, &shared, "reader-1");
    r2 = CMUTIL_ThreadCreate(reader, &shared, "reader-2");
    CMCall(r1, Start);
    CMCall(r2, Start);

    for (i = 0; i < 3; i++) {
        CMCall(shared.lock, WriteLock);
        shared.generation++;
        CMLogInfo("writer bumped generation to %d", shared.generation);
        CMCall(shared.lock, WriteUnlock);
        usleep(15000);
    }

    CMCall(r1, Join);
    CMCall(r2, Join);
    CMCall(shared.lock, Destroy);
}

int main(void)
{
    sample_init();

    sample_threads();
    sample_cond_and_semaphore();
    sample_rwlock();

    return sample_exit(0);
}
