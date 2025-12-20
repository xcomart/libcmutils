//
// Created by 박성진 on 25. 12. 15..
//

#include <stdio.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.log")

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);
    CMUTIL_LogAppender *apndr = NULL;
    CMUTIL_ConfLogger *logger = NULL;
    const char *pattern = "%d{%F %T.%Q} %P-[%-10t] (%-15F:%04L) [%p{TRACE=_,DEBUG=D,INFO=' ',WARN=W,FATAL=F}] %c : %m%n%ex";

    CMUTIL_LogSystem *lsys = CMUTIL_LogSystemCreate();
    ASSERT(lsys != NULL, "CMUTIL_LogSystemCreate");

    logger = CMCall(lsys, CreateLogger, NULL, CMLogLevel_Debug, CMTrue);
    ASSERT(logger != NULL, "LogSystem::CreateLogger");

    apndr = CMUTIL_LogConsoleAppenderCreate("Console", pattern);
    ASSERT(apndr != NULL, "LogConsoleAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Debug); apndr = NULL;

    apndr = CMUTIL_LogFileAppenderCreate("FileAppender", "log_file.txt", pattern);
    ASSERT(apndr != NULL, "LogFileAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Debug); apndr = NULL;

    apndr = CMUTIL_LogRollingFileAppenderCreate(
        "RollingFileAppender", "rolling_log.txt",
        CMLogTerm_Minute, "rolling/rolling_log.txt.%Y%m%d_%H%M",
        pattern);
    ASSERT(apndr != NULL, "LogRollingFileAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Debug); apndr = NULL;

    apndr = CMUTIL_LogSocketAppenderCreate(
        "SocketAppender", "0.0.0.0", 9999, pattern);
    ASSERT(apndr != NULL, "LogSocketAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Debug); apndr = NULL;

    CMUTIL_LogSystemSet(lsys); lsys = NULL;

    // You can view logs via the command "telnet localhost 9999" within 5 seconds

    for (int i=0; i<10; i++) {
        CMLogInfo("test log %d", i);
        usleep(500000);
    }

    for (int i=0; i<100; i++) {
        CMLogInfo("test fast log %d", i);
    }

    ir = 0;
END_POINT:
    if (lsys) CMCall(lsys, Destroy);
    if (!CMUTIL_Clear()) ir = 1;
    return ir;
}