/*
 * 11 - Timers
 *
 * Shows: CMUTIL_Timer with delay-based and absolute-time scheduling,
 *        repeating tasks, Cancel and Purge.
 *
 * Callbacks run on the timer's own small thread pool, so they must be
 * thread safe with respect to the rest of the program.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.timer")

typedef struct Ticker {
    const char *name;
    int count;
    CMUTIL_Mutex *mutex;
} Ticker;

static void on_tick(void *udata)
{
    Ticker *ticker = (Ticker*)udata;
    int count;

    CMCall(ticker->mutex, Lock);
    count = ++ticker->count;
    CMCall(ticker->mutex, Unlock);

    CMLogInfo("task '%s' fired (%d)", ticker->name, count);
}

int main(void)
{
    CMUTIL_Timer *timer;
    CMUTIL_TimerTask *once;
    CMUTIL_TimerTask *repeating;
    CMUTIL_TimerTask *absolute;
    CMUTIL_Mutex *mutex;
    Ticker single = { "single-shot", 0, NULL };
    Ticker repeated = { "every-100ms", 0, NULL };
    Ticker at_time = { "at-time", 0, NULL };
    struct timeval tv;

    sample_init();

    mutex = CMUTIL_MutexCreate();
    single.mutex = repeated.mutex = at_time.mutex = mutex;

    /* CMUTIL_TimerCreateEx(precision_ms, threads) tunes the tick resolution
     * and the worker count; CMUTIL_TimerCreate() uses 1 ms and 2 threads. */
    timer = CMUTIL_TimerCreate();

    SAMPLE_SECTION("scheduling");

    /* Run once, 200 ms from now. */
    once = CMCall(timer, ScheduleDelay, 200L, on_tick, &single);

    /* Run every 100 ms, starting 100 ms from now. elapse_skip = CMTrue
     * drops missed ticks instead of firing them back to back. */
    repeating = CMCall(timer, ScheduleDelayRepeat,
                       100L, 100L, CMTrue, on_tick, &repeated);

    /* The same thing against an absolute wall-clock time. */
    gettimeofday(&tv, NULL);
    tv.tv_sec += 1;
    absolute = CMCall(timer, ScheduleAtTime, &tv, on_tick, &at_time);

    CMLogInfo("waiting 1.2 seconds ...");
    usleep(1200 * 1000);

    SAMPLE_SECTION("cancelling");

    /* Cancel stops the task and frees the task handle. Do not call it after
     * the timer has been destroyed. */
    CMCall(repeating, Cancel);
    CMCall(absolute, Cancel);
    CMCall(once, Cancel);

    CMLogInfo("single-shot fired %d time(s)", single.count);
    CMLogInfo("repeating fired %d time(s)", repeated.count);
    CMLogInfo("absolute fired %d time(s)", at_time.count);

    /* Purge drops everything still scheduled. */
    CMCall(timer, Purge);
    CMCall(timer, Destroy);
    CMCall(mutex, Destroy);

    return sample_exit(0);
}
