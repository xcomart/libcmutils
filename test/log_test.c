//
// Created by 박성진 on 25. 12. 15..
//

#include <stdio.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.log")

int main() {
    int ir = -1;
    CMUTIL_Init(CMUTIL_MEM_TYPE);
    CMUTIL_LogAppender *apndr = NULL;
    CMUTIL_ConfLogger *logger = NULL;
    const char *pattern = "%d{%F %T.%Q} %P-[%-10t] (%-15F:%04L) [%p{TRACE=_,DEBUG=D,INFO=' ',WARN=W,FATAL=F}] %c : %m%n%ex";

    CMUTIL_LogSystem *lsys = CMUTIL_LogSystemCreate();
    ASSERT(lsys != NULL, "CMUTIL_LogSystemCreate");

    logger = CMCall(lsys, CreateLogger, NULL, CMLogLevel_Debug, CMTrue);
    ASSERT(logger != NULL, "LogSystem::CreateLogger");

    apndr = CMUTIL_LogConsoleAppenderCreate(
        "Console", pattern, CMTrue);
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

    // Setting what is already installed must not destroy it. Logging after
    // this would use freed memory if it did.
    CMUTIL_LogSystemSet(CMUTIL_LogSystemGet());
    ASSERT(CMUTIL_LogSystemGet() != NULL, "LogSystemSet with the current system");
    CMLogInfo("still logging through the system that was set twice");

    // Replacing a system releases the one it replaces, whichever way it is
    // installed - a system left behind here would show up as a leak in
    // CMUTIL_Clear() below.
    {
        CMUTIL_LogSystem *replacement = CMUTIL_LogSystemCreate();
        CMUTIL_ConfLogger *rlogger = CMCall(
            replacement, CreateLogger, NULL, CMLogLevel_Debug, CMTrue);
        CMUTIL_LogAppender *rapndr = CMUTIL_LogConsoleAppenderCreate(
            "Console", pattern, CMTrue);
        ASSERT(rlogger != NULL && rapndr != NULL, "replacement log system");
        CMCall(rlogger, AddAppender, rapndr, CMLogLevel_Debug);
        CMUTIL_LogSystemSet(replacement);
        CMLogInfo("logging through the replacement");
    }

    // The same must hold for a configure, which installs its own result.
    (void)CMUTIL_LogSystemConfigureFomJson("no_such_log_config.jsonc");
    ASSERT(CMUTIL_LogSystemGet() != NULL, "configure falls back and installs");
    CMLogInfo("logging through the fallback configuration");

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