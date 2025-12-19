//
// Created by 박성진 on 25. 12. 19..
//

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.timer")

const char *get_current_time() {
    time_t rawtime;
    struct tm *timeinfo;
    static char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%F %T", timeinfo);
    return buffer;
}

void timer_proc(void *udata) {
    CMLogInfo("task running time: %s", get_current_time());
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_TimerTask *task = NULL;
    CMUTIL_Timer *timer = CMUTIL_TimerCreate();
    ASSERT(timer, "TimerCreate");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    CMLogInfo("current time: %s", get_current_time());
    tv.tv_sec += 1;
    task = CMCall(timer, ScheduleAtTime, &tv, timer_proc, NULL);

    // wait 2 seconds
    usleep(2 * 1000 * 1000);

    gettimeofday(&tv, NULL);
    tv.tv_sec += 1;
    task = CMCall(timer, ScheduleAtRepeat, &tv, 500, CMTrue, timer_proc, NULL);

    // wait 2 seconds
    usleep(2 * 1000 * 1000);
    CMCall(task, Cancel);

    // wait 2 seconds
    usleep(2 * 1000 * 1000);

    ir = 0;
END_POINT:
    if (timer) CMCall(timer, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}