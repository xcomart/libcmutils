/*
MIT License

Copyright (c) 2020 Dennis Soungjin Park<xcomart@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "functions.h"

// #define LIBCMUTIL_VER_MAJOR     0
// #define LIBCMUTIL_VER_MINOR     1
// #define LIBCMUTIL_VER_PATCH     0

#define CMUTIL_TOSTR(x)         #x

#define LIBCMUTIL_VER LIBCMUTIL_VERSION_STRING

//static CMUTIL_Mutex *g_cmutil_init_mutex = NULL;
static int g_cmutil_init_cnt = 0;

void CMUTIL_Init(CMMemOper memoper)
{
    if (g_cmutil_init_cnt == 0) {
        CMUTIL_CallStackInit();
        CMUTIL_MemDebugInit(memoper);
        CMUTIL_ThreadInit();
        CMUTIL_StringBaseInit();
        CMUTIL_XmlInit();
        CMUTIL_NetworkInit();
        CMUTIL_LogInit();
        CMUTIL_LogSystem *log_system = CMUTIL_LogSystemGet();
        if (log_system) {
            CMUTIL_Logger *log = CMCall(log_system, GetLogger, "cmutil");
            if (log) {
                log->LogEx(log, CMLogLevel_Debug,
                    __FILE__, __LINE__, CMFalse,
                    "libcmutil version %s", CMUTIL_GetLibVersion());
            }
        } else {
            CMUTIL_LogFallback(CMLogLevel_Debug, __FILE__, __LINE__,
            "libcmutil version %s", CMUTIL_GetLibVersion());
        }
    }
    g_cmutil_init_cnt++;
}

void CMUTIL_Clear()
{
    if (g_cmutil_init_cnt > 0) {
        g_cmutil_init_cnt--;
        if (g_cmutil_init_cnt == 0) {
            CMUTIL_LogClear();
            CMUTIL_NetworkClear();
            CMUTIL_XmlClear();
            CMUTIL_StringBaseClear();
            CMUTIL_ThreadClear();
            CMUTIL_MemDebugClear();
            CMUTIL_CallStackClear();
        }
    }
}

void CMUTIL_UnusedP(void *p,...)
{
    (void)(p);
    // does nothing.
}

const char *CMUTIL_GetLibVersion()
{
    static char *verstr = LIBCMUTIL_VER;
    return verstr;
}
