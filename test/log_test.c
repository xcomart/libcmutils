//
// Created by 박성진 on 25. 12. 15..
//

#include <stdio.h>

#include "libcmutils.h"
#include "test.h"

CMUTIL_LogDefine("test.log")

static CMBool g_sock_running = CMTrue;

void *thread_proc(void *udata) {
    void *res = udata;
    CMUTIL_SocketAddr addr;
    CMUTIL_SocketAddrSet(&addr, "127.0.0.1", 9999);
    CMUTIL_Socket *sock = CMUTIL_SocketConnectWithAddr(&addr, 5000);
    if (sock) {
        CMSocketResult sr = CMSocketOk;
        CMUTIL_String *buffer = CMUTIL_StringCreate();
        while (g_sock_running) {
            sr = CMCall(sock, Read, buffer, 1024, 100);
            if (sr != CMSocketOk && sr != CMSocketTimeout) {
                CMLogError("socket read failed");
                res = NULL;
                break;
            }
            if (CMCall(buffer, GetSize) > 0) {
                printf("%s", CMCall(buffer, GetCString));
                CMCall(buffer, Clear);
            }
        }
        CMCall(sock, Close);
    } else {
        CMLogError("cannot connect to server");
        res = NULL;
    }
    return res;
}

int main() {
    int ir = -1;
    CMUTIL_Init(CMMemRecycle);
    CMUTIL_LogAppender *apndr = NULL;
    CMUTIL_ConfLogger *logger = NULL;
    const char *pattern = "%d %P-[%10t] (%-15F:%04L) [%-5p] %c - %m%ex%n";
    CMUTIL_Thread *tlogcli = NULL;

    CMUTIL_LogSystem *lsys = CMUTIL_LogSystemCreate();
    ASSERT(lsys != NULL, "CMUTIL_LogSystemCreate");

    logger = CMCall(lsys, CreateLogger, NULL, CMLogLevel_Trace, CMTrue);
    ASSERT(logger != NULL, "LogSystem::CreateLogger");

    apndr = CMUTIL_LogConsoleAppenderCreate("Console", pattern);
    ASSERT(apndr != NULL, "LogConsoleAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Trace); apndr = NULL;

    apndr = CMUTIL_LogFileAppenderCreate("FileAppender", "log_file.txt", pattern);
    ASSERT(apndr != NULL, "LogFileAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Trace); apndr = NULL;

    apndr = CMUTIL_LogRollingFileAppenderCreate(
        "RollingFileAppender", "rolling_log.txt",
        CMLogTerm_Minute, "rolling/rolling_log.txt.%Y%m%d_%H%M",
        pattern);
    ASSERT(apndr != NULL, "LogRollingFileAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Trace); apndr = NULL;

    apndr = CMUTIL_LogSocketAppenderCreate(
        "SocketAppender", "0.0.0.0", 9999, pattern);
    ASSERT(apndr != NULL, "LogSocketAppenderCreate");
    CMCall(apndr, SetAsync, 64);
    CMCall(logger, AddAppender, apndr, CMLogLevel_Trace); apndr = NULL;

    CMUTIL_LogSystemSet(lsys); lsys = NULL;

    tlogcli = CMUTIL_ThreadCreate(thread_proc, "name", "Cli");
    CMCall(tlogcli, Start);

    for (int i=0; i<100; i++) {
        CMLogInfo("test log %d", i);
    }

    ir = 0;
END_POINT:
    g_sock_running = CMFalse;
    if (tlogcli) CMCall(tlogcli, Join);
    if (lsys) CMCall(lsys, Destroy);
    if (!CMUTIL_Clear()) ir = 1;
    return ir;
}