/*
 * 10 - Thread pools
 *
 * Shows: CMUTIL_ThreadPoolCreate with a fixed and with a dynamic size,
 *        Execute, Wait and how work items report back through shared state.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.threadpool")

typedef struct Job {
    int id;
    CMUTIL_Mutex *mutex;
    int *completed;
} Job;

/* A pool task is a CMProcCB: void (*)(void *udata). */
static void run_job(void *udata)
{
    Job *job = (Job*)udata;
    CMUTIL_Thread *self = CMUTIL_ThreadSelf();

    CMLogInfo("job %d picked up by '%s'", job->id, CMCall(self, GetName));
    usleep(20000);

    CMSync(job->mutex, {
        (*job->completed)++;
    });
}

static void run_pool(int pool_size, const char *name)
{
    enum { JOB_COUNT = 8 };
    CMUTIL_ThreadPool *pool;
    CMUTIL_Mutex *mutex;
    Job jobs[JOB_COUNT];
    int completed = 0;
    int i;

    mutex = CMUTIL_MutexCreate();

    /* A pool_size of zero or less makes the pool grow on demand. */
    pool = CMUTIL_ThreadPoolCreate(pool_size, name);

    for (i = 0; i < JOB_COUNT; i++) {
        jobs[i].id = i;
        jobs[i].mutex = mutex;
        jobs[i].completed = &completed;
        /* Execute returns immediately; the job is queued. */
        CMCall(pool, Execute, run_job, &jobs[i]);
    }

    CMLogInfo("%s: queued %d jobs, %d done so far", name, JOB_COUNT, completed);

    /* Block until the queue has drained. */
    CMCall(pool, Wait);
    CMLogInfo("%s: after Wait, %d of %d done", name, completed, JOB_COUNT);

    CMCall(pool, Destroy);
    CMCall(mutex, Destroy);
}

int main(void)
{
    sample_init();

    SAMPLE_SECTION("fixed size pool - 3 threads");
    run_pool(3, "fixed");

    SAMPLE_SECTION("dynamic pool - grows one thread at a time");
    run_pool(-1, "dynamic");

    return sample_exit(0);
}
