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
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#if defined(MSWIN)
# include <process.h>
#endif

CMUTIL_LogDefine("cmutil.logger")

typedef enum CMUTIL_LogFormatItemType {
    LogFormatItem_DateTime,
    LogFormatItem_Millisec,
    LogFormatItem_String,
    LogFormatItem_LoggerName,
    LogFormatItem_ThreadId,
    LogFormatItem_FileName,
    LogFormatItem_LineNumber,
    LogFormatItem_LogLevel,
    LogFormatItem_Message,
    LogFormatItem_Stack,
    LogFormatItem_LineSeparator,

    // number of enumeration indicators
    LogFormatItem_Length
} CMUTIL_LogFormatItemType;

typedef enum CMUTIL_LogFormatType {
    LogType_DateTime = 0,
    LogType_LoggerName,
    LogType_ThreadId,
    LogType_ProcessId,
    LogType_FileName,
    LogType_LineNumber,
    LogType_LogLevel,
    LogType_Message,
    LogType_Environment,
    LogType_HostName,
    LogType_Stack,
    LogType_LineSeparator,

    // number of enumeration indicators
    LogType_Length
} CMUTIL_LogFormatType;

typedef struct CMUTIL_LogAppenderFormatItem CMUTIL_LogAppenderFormatItem;
struct CMUTIL_LogAppenderFormatItem {
    CMUTIL_LogFormatItemType    type;
    CMBool                      padleft;
    uint32_t                    precision[10];
    uint32_t                    precisioncnt;
    CMBool                      hasext;
    size_t                      length;
    ssize_t                     limit;
    CMUTIL_String               *data;
    CMUTIL_String               *data2;
    void                        *anything;
    CMUTIL_Mem                  *memst;
    CMBool                      zero;
};

typedef struct CMUTIL_LogAppenderBase CMUTIL_LogAppenderBase;
struct CMUTIL_LogAppenderBase {
    CMUTIL_LogAppender  base;
    CMBool              isasync;
    int                 dummy_padder;
    size_t              buffersz;
    void                (*Write)(CMUTIL_LogAppenderBase *appender,
                                 CMUTIL_String *logmsg,
                                 struct tm *logtm);
    char                *name;
    char                *spattern;
    CMUTIL_List         *pattern;
    CMUTIL_List         *buffer;
    CMUTIL_List         *flush_buffer;
    CMUTIL_Thread       *writer;
    CMUTIL_Mutex        *mutex;
    CMUTIL_Mem          *memst;
};

typedef struct CMUTIL_Logger_Internal CMUTIL_Logger_Internal;
struct CMUTIL_Logger_Internal {
    CMUTIL_Logger       base;
    CMLogLevel          minlevel;
    int                 dummy_padder;
    CMUTIL_String       *fullname;
    CMUTIL_StringArray  *namepath;
    CMUTIL_Array        *logrefs;
    long                spacer[50];
    CMUTIL_Mem          *memst;
    void                (*Destroy)(CMUTIL_Logger_Internal *logger);
};

typedef struct CMUTIL_LogPatternAppendFuncParam {
    CMUTIL_LogAppenderFormatItem    *item;
    CMLogLevel                      level;
    int                             linenum;
    CMUTIL_String                   *dest;
    CMUTIL_Logger                   *logger;
    CMUTIL_StringArray              *stack;
    struct tm                       *logtm;
    long                            millis;
    const char                      *fname;
    CMUTIL_String                   *logmsg;
} CMUTIL_LogPatternAppendFuncParam;

typedef void (*CMUTIL_LogPatternAppendFunc)(
    CMUTIL_LogPatternAppendFuncParam *params);
static CMUTIL_Map *g_cmutil_log_item_map = NULL;
static CMUTIL_LogFormatType g_cmutil_log_type_arr[LogType_Length] = {0,};
static CMUTIL_Map *g_cmutil_log_datefmt_map = NULL;
static CMUTIL_StringArray *g_cmutil_log_levels;
static CMUTIL_LogPatternAppendFunc
        g_cmutil_log_pattern_funcs[LogFormatItem_Length] = {0,};
static CMUTIL_Mutex *g_cmutil_logsystem_mutex = NULL;
static CMUTIL_LogSystem *__cmutil_logsystem = NULL;

CMUTIL_STATIC void CMUTIL_LogPathCreate(const char *fpath) {
    char pdir[1024] = {0,};
    const char *p, *q;
    p = strrchr(fpath, '/'); q = strrchr(fpath, '\\');
    if (p && q) {
        p = p > q? p:q;
    } else if (q) {
        p = q;
    }
    if (p) {
        strncat(pdir, fpath, (size_t)(p - fpath));
        CMUTIL_PathCreate(pdir, 0755);
    }
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_LogTokenPillOff(CMUTIL_Mem *memst, CMUTIL_String *src)
{
    if (strchr("'\"", CMCall(src, GetChar, 0))) {
        size_t sz = CMCall(src, GetSize) - 2;
        CMUTIL_String *tmp = CMUTIL_StringCreateInternal(memst, sz, NULL);
        const char *p = CMCall(src, GetCString) + 1;
        CMCall(tmp, AddNString, p, sz);
        CMCall(src, Destroy);
        return tmp;
    }
    return src;
}

CMUTIL_STATIC void CMUTIL_LogAppenderFormatItemDestroy(void *data)
{
    CMUTIL_LogAppenderFormatItem *ii = (CMUTIL_LogAppenderFormatItem*)data;
    if (ii->data)
        CMCall(ii->data, Destroy);
    if (ii->data2)
        CMCall(ii->data2, Destroy);
    if (ii->type == LogFormatItem_LogLevel && ii->anything)
        CMCall((CMUTIL_Map*)ii->anything, Destroy);
    ii->memst->Free(ii);
}

CMUTIL_STATIC CMUTIL_LogAppenderFormatItem *CMUTIL_LogAppenderItemCreate(
    CMUTIL_Mem *memst, CMUTIL_LogFormatItemType type)
{
    CMUTIL_LogAppenderFormatItem *res =
        memst->Alloc(sizeof(CMUTIL_LogAppenderFormatItem));
    memset(res, 0x0, sizeof(CMUTIL_LogAppenderFormatItem));
    res->type = type;
    res->memst = memst;
    return res;
}

static char g_cmutil_spaces[] = "                                             "
        "                                                                     "
        "                                                                     ";

CMUTIL_STATIC void CMUTIL_LogPatternAppendPadding(
    const CMUTIL_LogAppenderFormatItem *item,
    CMUTIL_String *dest,
    const char *data, size_t length)
{
    if (item->length > length) {
        if (item->padleft) {
            CMCall(dest, AddNString,
                        g_cmutil_spaces, item->length - length);
            CMCall(dest, AddNString,
                        data, length);
        }
        else {
            CMCall(dest, AddNString,
                        data, length);
            CMCall(dest, AddNString,
                        g_cmutil_spaces, item->length - length);
        }
    } else if (item->limit > 0 && item->limit < (ssize_t)length) {
        if (item->padleft) {
            const int skip = (int)(length - item->limit);
            CMCall(dest, AddNString, data+skip, item->limit);
        } else {
            CMCall(dest, AddNString, data, item->limit);
        }
    } else {
        CMCall(dest, AddNString, data, length);
    }
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendDateTime(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    char buf[50];
    size_t sz;
    const char *fmt = CMCall(params->item->data, GetCString);
    sz = strftime(buf, 50, fmt, params->logtm);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, sz);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendMillisec(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMCall(params->dest, AddPrint, "%03d", params->millis);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendString(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMCall(params->item->data, GetCString),
        CMCall(params->item->data, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLoggerName(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_Logger_Internal *ilog = (CMUTIL_Logger_Internal*)params->logger;
    CMUTIL_String *buf = CMUTIL_StringCreateInternal(ilog->memst, 50, NULL);
    const CMUTIL_String *curr = NULL;
    uint32_t i, tsize = (uint32_t)CMCall(ilog->namepath, GetSize);
    switch (params->item->precisioncnt) {
    case 0:
        CMCall(buf, AddAnother, ilog->fullname);
        break;
    case 1: {
        uint32_t lastcnt = params->item->precision[0];
        for (i = tsize - lastcnt; i < (tsize - 1); i++) {
            curr = CMCall(ilog->namepath, GetAt, i);
            CMCall(buf, AddAnother, curr);
            CMCall(buf, AddChar, '.');
        }
        curr = CMCall(ilog->namepath, GetAt, tsize - 1);
        CMCall(buf, AddAnother, curr);
        break;
    }
    case 2: {
        const uint32_t size = params->item->precision[0];
        for (i = 0; i < (tsize - 1); i++) {
            if (size) {
                size_t minsz = size;
                const char *currstr;
                curr = CMCall(ilog->namepath, GetAt, i);
                currstr = CMCall(curr, GetCString);
                if (CMCall(curr, GetSize) < size)
                    minsz = CMCall(curr, GetSize);
                CMCall(buf, AddNString, currstr, minsz);
            }
            CMCall(buf, AddChar, '.');
        }
        curr = CMCall(ilog->namepath, GetAt, tsize - 1);
        CMCall(buf, AddAnother, curr);
        break;
    }
    default:
        CMCall(buf, AddAnother, ilog->fullname);
        break;
    }

    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMCall(buf, GetCString),
        CMCall(buf, GetSize));

    CMCall(buf, Destroy);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendThreadId(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    // char buf[256];
    // CMUTIL_Thread *t = CMUTIL_ThreadSelf();
    // unsigned int tid = CMCall(t, GetId);
    // const char *name = CMCall(t, GetName);
    // uint32_t size = (uint32_t)sprintf(buf, "%s-%u", name, tid);
    // CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);

    const CMUTIL_Thread *t = CMUTIL_ThreadSelf();
    const char *name;
    if (t) {
        name = CMCall(t, GetName);
    } else {
        printf("ERROR!! cannot get thread object of %"PRIu64"\n",
            CMUTIL_ThreadSystemSelfId());
        name = "unknown";
    }
    const uint32_t size = (uint32_t)strlen(name);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, name, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendFileName(
    CMUTIL_LogPatternAppendFuncParam *params)
{
#if 0
    const char *p = strrchr(params->fname, '/');
    if (p == NULL) p = strrchr(params->fname, '\\');
    if (p == NULL) p = params->fname;
    else p++;
    const uint32_t size = (uint32_t)strlen(p);
#else
    uint32_t tsize = (uint32_t)strlen(params->fname), size = 0;
    const char *p = params->fname + (tsize - 1);
    while (p > params->fname && !strchr("/\\", *p)) {
        p--;
        size++;
    }
    if (strchr("/\\", *p))
        p++;
#endif
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, p, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLineNumber(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    char buf[20];
    uint32_t size;
    if (params->item->zero) {
        char fmtbuf[20];
        sprintf(fmtbuf, "%%0%dd", (int)params->item->length);
        size = (uint32_t)sprintf(buf, fmtbuf, params->linenum);
    } else {
        size = (uint32_t)sprintf(buf, "%d", params->linenum);
    }
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLogLevel(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    const CMUTIL_String *val = CMCall(g_cmutil_log_levels, GetAt, params->level);
    if (params->item->hasext) {
        CMUTIL_Map *lvlmap = (CMUTIL_Map*)params->item->anything;
        const CMUTIL_String *key = val;
        const char *skey = CMCall(key, GetCString);
        val = CMCall(lvlmap, Get, skey);
        if (val == NULL) val = key;
    }
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMCall(val, GetCString),
        CMCall(val, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendMessage(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMCall(params->logmsg, GetCString),
        CMCall(params->logmsg, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendStack(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    if (params->stack != NULL && CMCall(params->stack, GetSize) > 0) {
        uint32_t i;
        size_t maxlen = params->item->hasext ?
            params->item->precisioncnt : CMCall(params->stack, GetSize);
        for (i = 0; i < maxlen; i++) {
            const CMUTIL_String *item = CMCall(params->stack, GetAt, i);
            CMCall(params->dest, AddString, S_CRLF);
            CMCall(params->dest, AddAnother, item);
        }
    }
}

void CMUTIL_LogInit()
{
    int i;
    g_cmutil_log_item_map = CMUTIL_MapCreateInternal(
                CMUTIL_GetMem(), 50, CMFalse, NULL);
    for (i = 0; i < LogType_Length; i++)
        g_cmutil_log_type_arr[i] = (CMUTIL_LogFormatType)i;

#define LOGTYPE_ADD(a, b) CMCall(g_cmutil_log_item_map, Put, a, \
        &g_cmutil_log_type_arr[b], NULL)

    LOGTYPE_ADD("d"             , LogType_DateTime);
    LOGTYPE_ADD("date"          , LogType_DateTime);
    LOGTYPE_ADD("c"             , LogType_LoggerName);
    LOGTYPE_ADD("logger"        , LogType_LoggerName);
    LOGTYPE_ADD("t"             , LogType_ThreadId);
    LOGTYPE_ADD("tid"           , LogType_ThreadId);
    LOGTYPE_ADD("thread"        , LogType_ThreadId);
    LOGTYPE_ADD("P"             , LogType_ProcessId);
    LOGTYPE_ADD("pid"           , LogType_ProcessId);
    LOGTYPE_ADD("process"       , LogType_ProcessId);
    LOGTYPE_ADD("F"             , LogType_FileName);
    LOGTYPE_ADD("file"          , LogType_FileName);
    LOGTYPE_ADD("L"             , LogType_LineNumber);
    LOGTYPE_ADD("line"          , LogType_LineNumber);
    LOGTYPE_ADD("p"             , LogType_LogLevel);
    LOGTYPE_ADD("level"         , LogType_LogLevel);
    LOGTYPE_ADD("m"             , LogType_Message);
    LOGTYPE_ADD("msg"           , LogType_Message);
    LOGTYPE_ADD("message"       , LogType_Message);
    LOGTYPE_ADD("e"             , LogType_Environment);
    LOGTYPE_ADD("env"           , LogType_Environment);
    LOGTYPE_ADD("environment"   , LogType_Environment);
    LOGTYPE_ADD("ex"            , LogType_Stack);
    LOGTYPE_ADD("s"             , LogType_Stack);
    LOGTYPE_ADD("stack"         , LogType_Stack);
    LOGTYPE_ADD("n"             , LogType_LineSeparator);

    g_cmutil_log_datefmt_map = CMUTIL_MapCreate();
    CMCall(g_cmutil_log_datefmt_map, Put, "DEFAULT", "%F %T.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "ISO8601", "%FT%T.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put,
                "ISO8601_BASIC", "%Y%m%dT%H%M%S.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "ABSOLUTE", "%T.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "DATE", "%d %b %Y %T.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "COMPACT", "%Y%m%d%H%M%S.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "GENERAL", "%Y%m%d %H%M%S.%Q", NULL);
    CMCall(g_cmutil_log_datefmt_map, Put, "UNIX", "%s%Q", NULL);

    g_cmutil_log_levels = CMUTIL_StringArrayCreate();
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "TRACE"));
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "DEBUG"));
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "INFO" ));
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "WARN" ));
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "ERROR"));
    CMCall(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "FATAL"));

#define PATTERN_APPENDF(a) g_cmutil_log_pattern_funcs[LogFormatItem_##a]=\
        CMUTIL_LogPatternAppend##a
    PATTERN_APPENDF(DateTime);
    PATTERN_APPENDF(Millisec);
    PATTERN_APPENDF(String);
    PATTERN_APPENDF(LoggerName);
    PATTERN_APPENDF(ThreadId);
    PATTERN_APPENDF(FileName);
    PATTERN_APPENDF(LineNumber);
    PATTERN_APPENDF(LogLevel);
    PATTERN_APPENDF(Message);
    PATTERN_APPENDF(Stack);

    g_cmutil_logsystem_mutex = CMUTIL_MutexCreate();

    // initialize default log system
    (void)CMUTIL_LogSystemGet();
}

void CMUTIL_LogClear()
{
    CMCall(g_cmutil_log_item_map, Destroy);
    g_cmutil_log_item_map = NULL;
    CMCall(g_cmutil_log_datefmt_map, Destroy);
    g_cmutil_log_datefmt_map = NULL;
    CMCall(g_cmutil_log_levels, Destroy);
    g_cmutil_log_levels = NULL;
    CMCall(g_cmutil_logsystem_mutex, Destroy);
    g_cmutil_logsystem_mutex = NULL;
    if (__cmutil_logsystem) {
        CMCall(__cmutil_logsystem, Destroy);
        __cmutil_logsystem = NULL;
    }
}

CMUTIL_STATIC const char *CMUTIL_LogAppenderBaseGetName(
    const CMUTIL_LogAppender *appender)
{
    const CMUTIL_LogAppenderBase *iap =
            (const CMUTIL_LogAppenderBase*)appender;
    return iap->name;
}

CMUTIL_STATIC void CMUTIL_LogAppenderStringDestroyer(void *data)
{
    CMCall((CMUTIL_String*)data, Destroy);
}

typedef struct CMUTIL_LogAppderAsyncItem {
    CMUTIL_String *log;
    struct tm logtm;
    CMUTIL_Mem *memst;
} CMUTIL_LogAppderAsyncItem;


CMUTIL_STATIC void CMUTIL_LogAppderAsyncItemDestroy(
        CMUTIL_LogAppderAsyncItem *item)
{
    if (item) {
        if (item->log) CMCall(item->log, Destroy);
        item->memst->Free(item);
    }
}

CMUTIL_STATIC void *CMUTIL_LogAppenderAsyncWriter(void *udata)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)udata;
    while (iap->isasync) {
        CMCall((CMUTIL_LogAppender*)iap, Flush);
        USLEEP(100000);	// sleep 100 milliseconds
    }
    CMCall((CMUTIL_LogAppender*)iap, Flush);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_LogAppenderBaseSetAsync(
    CMUTIL_LogAppender *appender, size_t buffersz)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMCall(iap->mutex, Lock);
    if (!iap->isasync) {
        iap->buffersz = buffersz;
        iap->isasync = CMTrue;
        if (iap->buffer == NULL)
            iap->buffer = CMUTIL_ListCreateInternal(
                        iap->memst, CMUTIL_LogAppenderStringDestroyer);
        if (iap->flush_buffer == NULL)
            iap->flush_buffer = CMUTIL_ListCreateInternal(
                        iap->memst, CMUTIL_LogAppenderStringDestroyer);
        if (iap->writer == NULL) {
            char namebuf[256];
            sprintf(namebuf, "%s-AsyncWriter", iap->name);
            iap->writer = CMUTIL_ThreadCreateInternal(
                        iap->memst, CMUTIL_LogAppenderAsyncWriter,
                        appender, namebuf);
            CMCall(iap->writer, Start);
        }
    }
    CMCall(iap->mutex, Unlock);
}

CMUTIL_STATIC CMUTIL_LogAppderAsyncItem *CMUTIL_LogAppderAsyncItemCreate(
    CMUTIL_Mem *memst, CMUTIL_String *log, struct tm *logtm)
{
    CMUTIL_LogAppderAsyncItem *res =
            memst->Alloc(sizeof(CMUTIL_LogAppderAsyncItem));
    memcpy(&(res->logtm), logtm, sizeof(struct tm));
    res->log = log;
    res->memst = memst;
    return res;
}

CMUTIL_STATIC void CMUTIL_LogAppenderBaseAppend(
    CMUTIL_LogAppender *appender,
    CMUTIL_Logger *logger,
    CMLogLevel level,
    CMUTIL_StringArray *stack,
    const char *fname,
    int linenum,
    CMUTIL_String *logmsg)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_Iterator *iter = NULL;
    struct tm currtm;
    CMUTIL_LogPatternAppendFuncParam param;
    struct timeval tv;

#if defined(MSWIN)
    time_t nowtm = time(NULL);
    gettimeofday(&tv, NULL);
    localtime_s(&currtm, &nowtm);
#else
    gettimeofday(&tv, NULL);
    localtime_r((time_t*)&(tv.tv_sec), &currtm);
#endif

    memset(&param, 0x0, sizeof(param));

    param.dest = CMUTIL_StringCreateInternal(iap->memst, 64, NULL);
    param.logger = logger;
    param.level = level;
    param.stack = stack;
    param.logtm = &currtm;
    param.millis = tv.tv_usec / 1000;
    param.fname = fname;
    param.linenum = linenum;
    param.logmsg = logmsg;
    //param.args = args;

    // create log string
    iter = CMCall(iap->pattern, Iterator);
    while (CMCall(iter, HasNext)) {
        param.item = (CMUTIL_LogAppenderFormatItem*)CMCall(iter, Next);
        g_cmutil_log_pattern_funcs[param.item->type](&param);
    }
    CMCall(iter, Destroy);

    if (iap->isasync) {
        CMCall(iap->mutex, Lock);
        CMCall(iap->buffer, AddTail,
            CMUTIL_LogAppderAsyncItemCreate(
                        iap->memst, param.dest, &currtm));
        if (CMCall(iap->buffer, GetSize) >= iap->buffersz) {
            CMCall(appender, Flush);
        }
        CMCall(iap->mutex, Unlock);
    }
    else {
        CMCall(iap->mutex, Lock);
        CMCall(iap, Write, param.dest, &currtm);
        CMCall(iap->mutex, Unlock);
        CMCall(param.dest, Destroy);
    }
}

CMUTIL_STATIC CMUTIL_LogAppenderFormatItem *CMUTIL_LogAppenderItemString(
    CMUTIL_Mem *memst, const char *a, size_t length)
{
    CMUTIL_LogAppenderFormatItem *res =
        CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
    res->data = CMUTIL_StringCreateInternal(memst, length, NULL);
    CMCall(res->data, AddNString, a, length);
    return res;
}

CMUTIL_STATIC void CMUTIL_LogPatternLogLevelParse(
    CMUTIL_LogAppenderFormatItem *item, const char *extra)
{
    CMUTIL_Map *map = CMUTIL_MapCreateInternal(
                item->memst, 10, CMFalse,
                CMUTIL_LogAppenderStringDestroyer);
    CMUTIL_StringArray *args = CMUTIL_StringSplitInternal(
                item->memst, extra, ",");
    for (uint32_t i = 0; i < CMCall(args, GetSize); i++) {
        const CMUTIL_String *str = CMCall(args, GetAt, i);
        CMUTIL_StringArray *arr = CMUTIL_StringSplitInternal(
                    item->memst, CMCall(str, GetCString), "=");
        if (CMCall(arr, GetSize) == 2) {
            const CMUTIL_String *key = CMCall(arr, GetAt, 0);
            const char *ks = CMCall(key, GetCString);
            const char *vs = CMCall(arr, GetCString, 1);
            if (strcasecmp(ks, "length") == 0) {
                const long slen = strtol(vs, NULL, 10);
                if (slen > 0) {
                    const uint32_t len = (uint32_t)slen;
                    CMUTIL_Iterator *iter =
                        CMCall(g_cmutil_log_levels, Iterator);
                    while (CMCall(iter, HasNext)) {
                        const CMUTIL_String *v =
                            (CMUTIL_String*)CMCall(iter, Next);
                        const char *skey = CMCall(v, GetCString);
                        if (CMCall(map, Get, skey) == NULL) {
                            CMUTIL_String *d = CMCall(v, Substring, 0, len);
                            CMUTIL_String *prev = NULL;
                            CMCall(map, Put, skey, d, (void**)&prev);
                            if (prev) CMCall(prev, Destroy);
                        }
                    }
                    CMCall(iter, Destroy);
                }
            }
            else if (strcasecmp(ks, "lowerCase") == 0) {
                CMUTIL_Iterator *iter =
                        CMCall(g_cmutil_log_levels, Iterator);
                while (CMCall(iter, HasNext)) {
                    const CMUTIL_String *v =(CMUTIL_String*)CMCall(iter, Next);
                    const char *skey = CMCall(v, GetCString);
                    if (CMCall(map, Get, skey) == NULL) {
                        CMUTIL_String *d = CMCall(v, ToLower);
                        CMUTIL_String *prev = NULL;
                        CMCall(map, Put, skey, d, (void**)&prev);
                        if (prev) CMCall(prev, Destroy);
                    }
                }
                CMCall(iter, Destroy);
            }
            else {
                CMUTIL_String *val = CMCall(arr, RemoveAt, 1);
                CMUTIL_String *prev = NULL;
                // pill-off quotations
                val = CMUTIL_LogTokenPillOff(item->memst, val);
                CMCall(map, Put, ks, val, (void**)&prev);
                if (prev) CMCall(prev, Destroy);
            }
        }
        else {
            // TODO: error report
        }
        CMCall(arr, Destroy);
    }

    CMCall(args, Destroy);
    item->anything = map;
}

CMUTIL_STATIC CMBool CMUTIL_LogPatternParse(
    CMUTIL_Mem *memst, CMUTIL_List *list, const char *pattern)
{
    const char *p, *q, *r;
    p = pattern;

    // check the occurrence of '%'
    while ((q = strchr(p, '%')) != NULL) {
        char buf[128] = { 0, };
        char fmt[20];
        uint32_t length = 0;
        int limit = -1;
        CMBool padleft = CMFalse;
        CMBool hasext = CMFalse;
        CMBool zero = CMFalse;
        CMUTIL_LogFormatType type;
        CMUTIL_LogAppenderFormatItem *item = NULL;
        void *tvoid = NULL;

        // add a former string to a pattern list
        if (q > p)
            CMCall(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, p, (uint32_t)(q - p)));
        q++;
        // check escape character
        if (*q == '%') {
            CMCall(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, "%", 1));
            p = q + 1;
            continue;
        }
        // check the padding length
        if (strchr("+-0123456789", *q)) {
            char *br = buf;
            if (strchr("+-", *q)) {
                if (*q == '-')
                    padleft = CMTrue;
                q++;
            }
            while (strchr("0123456789.", *q))
                *br++ = *q++;
            *br = 0x0;
            br = strchr(buf, '.');
            if (br) {
                *br = 0x0;
                limit = (int)strtol(br + 1, NULL, 10);
            }
            length = strtol(buf, NULL, 10);
            zero = *buf == '0'? CMTrue : CMFalse;
        }
        // check the pattern item
        q = CMUTIL_StrNextToken(
                    buf, 20, q, " \t\r\n{}[]:;,.!@#$%^&*()-+_=/\\|");
        tvoid = CMCall(g_cmutil_log_item_map, Get, buf);
        if (!tvoid) {
            printf("unknown pattern %%%s\n", buf);
            return CMFalse;
        }
        type = *(CMUTIL_LogFormatType*)tvoid;
        *buf = 0x0;
        if (*q == '{') {
            r = q + 1;
            q = strchr(r, '}');
            if (q) {
                strncat(buf, r, (size_t)(q - r));
                q++;
                hasext = CMTrue;
            }
            else {
                printf("pattern brace mismatch.\n");
                return CMFalse;
            }
        }
        switch (type) {
        case LogType_DateTime:
            if (hasext) {
                const char *milli = NULL;

                if (strchr(buf, '%') == NULL) {
                    const char *dfmt = (char*)CMCall(
                            g_cmutil_log_datefmt_map, Get, buf);
                    if (dfmt == NULL) {
                        printf("invalid date pattern: %s\n", buf);
                        return CMFalse;
                    }
                    strcpy(buf, dfmt);
                }
                if (strstr(buf, "%q")) milli = "%q";
                else if (strstr(buf, "%Q")) milli = "%Q";
                if (milli) {
                    CMUTIL_StringArray *arr =
                            CMUTIL_StringSplitInternal(memst, buf, milli);
                    const uint32_t size = (uint32_t)CMCall(arr, GetSize);
                    for (uint32_t i = 0; i < size; i++) {
                        CMUTIL_String *s = CMCall(arr, RemoveAt, 0);
                        if (CMCall(s, GetSize) > 0) {
                            item = CMUTIL_LogAppenderItemCreate(
                                        memst, LogFormatItem_DateTime);
                            item->data = s;
                            CMCall(list, AddTail, item);
                        }
                        else {
                            CMCall(s, Destroy);
                        }
                        if (i < (size - 1)){
                            item = CMUTIL_LogAppenderItemCreate(
                                        memst, LogFormatItem_Millisec);
                            item->hasext =
                                *(milli + 1) == 'Q' ?
                                        CMTrue : CMFalse;
                            CMCall(list, AddTail, item);
                        }
                    }
                    CMCall(arr, Destroy);
                }
                else {
                    item = CMUTIL_LogAppenderItemCreate(
                                memst, LogFormatItem_DateTime);
                    item->data = CMUTIL_StringCreateInternal(memst, 20, buf);
                    CMCall(list, AddTail, item);
                }
            }
            else {
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_DateTime);
                item->data = CMUTIL_StringCreateInternal(memst, 7, "%F %T.");
                item->hasext = CMTrue;
                CMCall(list, AddTail, item);
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_Millisec);
                item->hasext = CMTrue;
                CMCall(list, AddTail, item);
            }
            item = NULL;
            break;
        case LogType_LoggerName: {
            item = CMUTIL_LogAppenderItemCreate(
                        memst, LogFormatItem_LoggerName);
            if (hasext) {
                r = p = buf;
                while ((r = strchr(p, '.')) != NULL) {
                    char temp[10] = { 0, };
                    if (r == p) {
                        item->precision[item->precisioncnt] = 0;
                    }
                    else {
                        strncat(temp, p, (size_t)(r - p));
                        item->precision[item->precisioncnt] =
                                (uint32_t)atoi(temp);
                    }
                    item->precisioncnt++;
                    p = r + 1;
                }
                if (item->precisioncnt == 0) {
                    item->precision[item->precisioncnt] = (uint32_t)atoi(buf);
                }
                else {
                    item->precision[item->precisioncnt] = 0;
                }
                item->precisioncnt++;
            }
            break;
        }
        case LogType_ThreadId:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_ThreadId);
            break;
        case LogType_ProcessId:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
            item->data = CMUTIL_StringCreateInternal(memst, 50, NULL);
            if (length != 0) {
                if (zero) {
                    sprintf(fmt, "%%0%dld", length);
                } else {
                    sprintf(fmt, "%%%s%dld", padleft ? "" : "-", length);
                }
            }
            else {
                strcpy(fmt, "%ld");
            }
            CMCall(item->data, AddPrint, fmt, GETPID());
            break;
        case LogType_FileName:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_FileName);
            break;
        case LogType_LineNumber:
            item = CMUTIL_LogAppenderItemCreate(
                        memst, LogFormatItem_LineNumber);
            break;
        case LogType_LogLevel:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_LogLevel);
            if (hasext)
                CMUTIL_LogPatternLogLevelParse(item, buf);
            break;
        case LogType_Message:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_Message);
            break;
        case LogType_Environment:
            if (hasext) {
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_String);
                item->data = CMUTIL_StringCreateInternal(memst, 50, NULL);
                if (length != 0) {
                    sprintf(fmt, "%%%s%ds", padleft ? "" : "-", length);
                }
                else {
                    strcpy(fmt, "%s");
                }
                CMCall(item->data, AddPrint, fmt, getenv(buf));
            }
            else {
                // TODO: error report
            }
            break;
        case LogType_HostName: {
            char hbuf[1024];
            item = CMUTIL_LogAppenderItemCreate(
                        memst, LogFormatItem_String);
            item->data = CMUTIL_StringCreateInternal(memst, 50, NULL);
            if (gethostname(hbuf, 1024) == 0) {
                CMCall(item->data, AddString, hbuf);
            } else {
                CMCall(item->data, AddString, "unknown");
            }
            break;
        }
        case LogType_Stack:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_Stack);
            if (hasext)
                item->precisioncnt = (uint32_t)atoi(buf);
            break;
        case LogType_LineSeparator:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
            item->data = CMUTIL_StringCreateInternal(memst, 3, NULL);
            CMCall(item->data, AddString, S_CRLF);
            break;
        default:
            printf("unknown pattern type.\n");
            return CMFalse;
        }
        if (item) {
            item->length = length;
            item->limit = limit;
            item->padleft = padleft;
            item->hasext = hasext;
            item->zero = zero;
            CMCall(list, AddTail, item);
        }
        p = q;
    }
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogAppenderBaseInit(
    CMUTIL_LogAppenderBase *iap,
    const char *pattern,
    const char *name,
    void(*WriteFn)(CMUTIL_LogAppenderBase *appender,
            CMUTIL_String *logmsg, struct tm *logtm),
        void(*FlushFn)(CMUTIL_LogAppender *appender),
        void(*DestroyFn)(CMUTIL_LogAppender *appender))
{
    iap->pattern = CMUTIL_ListCreateInternal(
                iap->memst, CMUTIL_LogAppenderFormatItemDestroy);
    iap->spattern = iap->memst->Strdup(pattern);
    if (!CMUTIL_LogPatternParse(iap->memst, iap->pattern, pattern))
        return CMFalse;
    iap->Write = WriteFn;
    iap->name = iap->memst->Strdup(name);
    iap->mutex = CMUTIL_MutexCreateInternal(iap->memst);
    iap->base.GetName = CMUTIL_LogAppenderBaseGetName;
    iap->base.SetAsync = CMUTIL_LogAppenderBaseSetAsync;
    iap->base.Append = CMUTIL_LogAppenderBaseAppend;
    iap->base.Flush = FlushFn;
    iap->base.Destroy = DestroyFn;
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogAppenderPatternRebuild(
    CMUTIL_LogAppenderBase *iap)
{
    if (iap->pattern) CMCall(iap->pattern, Destroy);
    iap->pattern = CMUTIL_ListCreateInternal(
                iap->memst, CMUTIL_LogAppenderFormatItemDestroy);
    return CMUTIL_LogPatternParse(iap->memst, iap->pattern, iap->spattern);
}

CMUTIL_STATIC CMUTIL_Mem *CMUTIL_LogAppenderBaseClean(
        CMUTIL_LogAppenderBase *iap)
{
    CMUTIL_Mem *res = NULL;
    if (iap) {
        res = iap->memst;
        if (iap->buffer && CMCall(iap->buffer, GetSize) > 0) {
            // we need to flush items
            CMCall((CMUTIL_LogAppender*)iap, Flush);
        }
        if (iap->writer) {
            iap->isasync = CMFalse;
            CMCall(iap->writer, Join);
        }
        if (iap->spattern)
            res->Free(iap->spattern);
        if (iap->buffer)
            CMCall(iap->buffer, Destroy);
        if (iap->flush_buffer)
            CMCall(iap->flush_buffer, Destroy);
        if (iap->pattern)
            CMCall(iap->pattern, Destroy);
        if (iap->name)
            res->Free(iap->name);
        if (iap->mutex)
            CMCall(iap->mutex, Destroy);
   }
    return res;
}

typedef struct CMUTIL_LogConsoleAppender {
    CMUTIL_LogAppenderBase  base;
    FILE                    *out;
} CMUTIL_LogConsoleAppender;

CMUTIL_STATIC void CMUTIL_LogConsoleAppenderWriter(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_LogConsoleAppender *cap = (CMUTIL_LogConsoleAppender*)appender;
    CMUTIL_UNUSED(logtm);
    CMSync(iap->mutex, fprintf(cap->out, "%s", CMCall(logmsg, GetCString)););
    CMCall((CMUTIL_LogAppender*)appender, Flush);
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenderFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_LogConsoleAppender *cap = (CMUTIL_LogConsoleAppender*)appender;
    if (iap->isasync) {
        CMSync(iap->mutex, {
            CMCall(iap->flush_buffer, MoveAll, iap->buffer);
            CMUTIL_List *list = iap->flush_buffer;

            while (CMCall(list, GetSize) > 0) {
                CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                        CMCall(list, RemoveFront);
                fprintf(cap->out, "%s", CMCall(log->log, GetCString));
                CMUTIL_LogAppderAsyncItemDestroy(log);
            }
        });
    }
    fflush(cap->out);
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenderDestroy(
        CMUTIL_LogAppender *appender)
{
    if (appender) {
        CMUTIL_Mem *memst =
                CMUTIL_LogAppenderBaseClean((CMUTIL_LogAppenderBase*)appender);
        memst->Free(appender);
    }
}

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name, const char *pattern,
        CMBool use_stderr)
{
    CMUTIL_LogConsoleAppender *res =
            memst->Alloc(sizeof(CMUTIL_LogConsoleAppender));
    memset(res, 0x0, sizeof(CMUTIL_LogConsoleAppender));
    res->base.memst = memst;
    res->out = use_stderr ? stderr : stdout;
    if (!CMUTIL_LogAppenderBaseInit(
                &res->base, pattern, name,
                CMUTIL_LogConsoleAppenderWriter,
                CMUTIL_LogConsoleAppenderFlush,
                CMUTIL_LogConsoleAppenderDestroy)) {
        CMCall((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }
    return (CMUTIL_LogAppender*)res;
}

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreate(
        const char *name, const char *pattern, CMBool use_stderr)
{
    return CMUTIL_LogConsoleAppenderCreateInternal(
                CMUTIL_GetMem(), name, pattern, use_stderr);
}

typedef struct CMUTIL_LogFileAppender {
    CMUTIL_LogAppenderBase	base;
    char *fpath;
} CMUTIL_LogFileAppender;

CMUTIL_STATIC void CMUTIL_LogFileAppenderWriter(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    CMUTIL_LogFileAppender *iap = (CMUTIL_LogFileAppender*)appender;
    FILE *fp = NULL;
    int count = 0;
    CMUTIL_UNUSED(logtm);
    CMCall(appender->mutex, Lock);
RETRY:
    fp = fopen(iap->fpath, "ab");
    if (fp) {
        fprintf(fp, "%s", CMCall(logmsg, GetCString));
        fclose(fp);
    } else if (!count) {
        CMUTIL_File *f = CMUTIL_FileCreateInternal(
                    appender->memst, iap->fpath);
        if (CMCall(f, IsExists)) {
            printf("cannot open log file '%s'\n", iap->fpath);
            CMCall(f, Destroy);
        } else {
            CMCall(f, Destroy);
            CMUTIL_LogPathCreate(iap->fpath);
            count++;
            goto RETRY;
        }
    } else {
        printf("cannot open log file '%s'\n", iap->fpath);
    }
    CMCall(appender->mutex, Unlock);
}

CMUTIL_STATIC void CMUTIL_LogFileAppenderFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogFileAppender *iap = (CMUTIL_LogFileAppender*)appender;
    FILE *fp = NULL;
    CMSync(iap->base.mutex, {
        fp = fopen(iap->fpath, "ab");
        if (fp) {
            CMCall(iap->base.flush_buffer, MoveAll, iap->base.buffer);
            CMUTIL_List *list = iap->base.flush_buffer;
            while (CMCall(list, GetSize) > 0) {
                CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                        CMCall(list, RemoveFront);
                fprintf(fp, "%s", CMCall(log->log, GetCString));
                CMUTIL_LogAppderAsyncItemDestroy(log);
            }
            fclose(fp);
        }
    });
}

CMUTIL_STATIC void CMUTIL_LogFileAppenderDestroy(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogFileAppender *iap = (CMUTIL_LogFileAppender*)appender;
    if (iap) {
        CMUTIL_Mem *memst = CMUTIL_LogAppenderBaseClean(&(iap->base));
        if (iap->fpath)
            memst->Free(iap->fpath);
        memst->Free(iap);
    }
}

CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name,
        const char *fpath, const char *pattern)
{
    CMUTIL_LogFileAppender *res =
            memst->Alloc(sizeof(CMUTIL_LogFileAppender));
    memset(res, 0x0, sizeof(CMUTIL_LogFileAppender));
    res->base.memst = memst;
    if (!CMUTIL_LogAppenderBaseInit(
                (CMUTIL_LogAppenderBase*)res, pattern, name,
                CMUTIL_LogFileAppenderWriter,
                CMUTIL_LogFileAppenderFlush,
                CMUTIL_LogFileAppenderDestroy)) {
        CMCall((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }
    res->fpath = memst->Strdup(fpath);
    CMUTIL_LogPathCreate(fpath);
    return (CMUTIL_LogAppender*)res;
}

CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreate(
    const char *name,
    const char *fpath,
    const char *pattern)
{
    return CMUTIL_LogFileAppenderCreateInternal(
                CMUTIL_GetMem(), name, fpath, pattern);
}

typedef struct CMUTIL_LogRollingFileAppender {
    CMUTIL_LogAppenderBase base;
    CMLogTerm       logterm;
    int             dummy_padder;
    char            *fpath;
    char            *rollpath;
    struct tm       lasttm;
    CMPtrDiff       tmst;
    CMPtrDiff       tmlen;
} CMUTIL_LogRollingFileAppender;

CMUTIL_STATIC CMBool CMUTIL_LogRollingFileAppenderIsChanged(
        CMUTIL_LogRollingFileAppender *appender, struct tm *curr)
{
    return memcmp(
                (char*)&(appender->lasttm) + appender->tmst,
                (char*)curr + appender->tmst,
                (size_t)appender->tmlen) == 0? CMFalse:CMTrue;
}

CMUTIL_STATIC void CMUTIL_LogRollingFileAppenderRolling(
        CMUTIL_LogRollingFileAppender *appender,
        struct tm *curr)
{
    CMUTIL_File *f = NULL;
    char tfname[1024] = {0,};
    int i = 0;
    const size_t sz = strftime(
        tfname, 1024, appender->rollpath, &(appender->lasttm));
    tfname[sz] = '\0';
    CMUTIL_LogPathCreate(tfname);

    f = CMUTIL_FileCreateInternal(appender->base.memst, tfname);
    while (CMCall(f, IsExists)) {
        char buf[2048];
        sprintf(buf, "%s-%d", tfname, i++);
        CMCall(f, Destroy);
        f = CMUTIL_FileCreateInternal(appender->base.memst, buf);
    }
    rename(appender->fpath, CMCall(f, GetFullPath));
    CMCall(f, Destroy);
    memcpy(&(appender->lasttm), curr, sizeof(struct tm));
}

CMUTIL_STATIC void CMUTIL_LogRollingFileAppenderWrite(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    CMUTIL_LogRollingFileAppender *iap =
            (CMUTIL_LogRollingFileAppender*)appender;
    FILE *fp = NULL;
    CMSync(iap->base.mutex, {
        if (CMUTIL_LogRollingFileAppenderIsChanged(iap, logtm))
            CMUTIL_LogRollingFileAppenderRolling(iap, logtm);
        fp = fopen(iap->fpath, "ab");
        if (fp) {
            fprintf(fp, "%s", CMCall(logmsg, GetCString));
            fclose(fp);
        } else {
            printf("cannot open log file '%s'.\n", iap->fpath);
        }
    });
}

CMUTIL_STATIC void CMUTIL_LogRollingFileAppenderFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogRollingFileAppender *iap =
            (CMUTIL_LogRollingFileAppender*)appender;
    CMSync(iap->base.mutex, {
        CMCall(iap->base.flush_buffer, MoveAll, iap->base.buffer);
        CMUTIL_List *list = iap->base.flush_buffer;
        if (CMCall(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *item =
                    (CMUTIL_LogAppderAsyncItem*)CMCall(list, GetFront);
            if (CMUTIL_LogRollingFileAppenderIsChanged(iap, &item->logtm))
                CMUTIL_LogRollingFileAppenderRolling(iap, &item->logtm);
            FILE *fp = fopen(iap->fpath, "ab");
            if (fp) {
                if (CMCall(list, GetSize) > 0) {
                    while (CMCall(list, GetSize) > 0) {
                        item = (CMUTIL_LogAppderAsyncItem*)CMCall(list, RemoveFront);
                        if (CMUTIL_LogRollingFileAppenderIsChanged(iap, &item->logtm)) {
                            fclose(fp);
                            CMUTIL_LogRollingFileAppenderRolling(iap, &item->logtm);
                            fp = fopen(iap->fpath, "ab");
                        }
                        fprintf(fp, "%s", CMCall(item->log, GetCString));
                        CMUTIL_LogAppderAsyncItemDestroy(item);
                    }
                }
                fclose(fp);
            }
        }
    });
}

CMUTIL_STATIC void CMUTIL_LogRollingFileAppenderDestroy(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogRollingFileAppender *iap =
            (CMUTIL_LogRollingFileAppender*)appender;
    if (iap) {
        CMUTIL_Mem *memst =
                CMUTIL_LogAppenderBaseClean((CMUTIL_LogAppenderBase*)iap);
        if (iap->fpath) memst->Free(iap->fpath);
        if (iap->rollpath) memst->Free(iap->rollpath);
        memst->Free(iap);
    }
}

CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name,
        const char *fpath, CMLogTerm logterm,
        const char *rollpath, const char *pattern)
{
    CMPtrDiff base;
    CMUTIL_LogRollingFileAppender *res =
            memst->Alloc(sizeof(CMUTIL_LogRollingFileAppender));
    struct tm curr;
    time_t ctime, ptime;
    CMUTIL_File *file = NULL;

    memset(res, 0x0, sizeof(CMUTIL_LogRollingFileAppender));
    res->base.memst = memst;
    if (!CMUTIL_LogAppenderBaseInit(
                (CMUTIL_LogAppenderBase*)res, pattern, name,
                CMUTIL_LogRollingFileAppenderWrite,
                CMUTIL_LogRollingFileAppenderFlush,
                CMUTIL_LogRollingFileAppenderDestroy)) {
        CMCall((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }

    res->fpath = memst->Strdup(fpath);
    res->rollpath = memst->Strdup(rollpath);

    base = (CMPtrDiff)&(res->lasttm);
    switch (logterm) {
    case CMLogTerm_Year:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_year) - base; break;
    case CMLogTerm_Month:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_mon) - base; break;
    case CMLogTerm_Date:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_mday) - base; break;
    case CMLogTerm_Hour:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_hour) - base; break;
    case CMLogTerm_Minute:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_min) - base; break;
    }
    res->tmlen = (CMPtrDiff)&(res->lasttm.tm_wday) - base - res->tmst;

    file = CMUTIL_FileCreateInternal(memst, fpath);
    ctime = time(NULL);
    localtime_r(&ctime, &curr);
    if (CMCall(file, IsExists)) {
        ptime = CMCall(file, ModifiedTime);
        localtime_r(&ptime, &(res->lasttm));
        if (CMUTIL_LogRollingFileAppenderIsChanged(res, &curr))
            CMUTIL_LogRollingFileAppenderRolling(res, &curr);
    } else {
        localtime_r(&ctime, &(res->lasttm));
    }
    CMCall(file, Destroy);
    CMUTIL_LogPathCreate(fpath);
    return (CMUTIL_LogAppender*)res;
}

CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreate(
    const char *name,
    const char *fpath,
    CMLogTerm logterm,
    const char *rollpath,
    const char *pattern)
{
    return CMUTIL_LogRollingFileAppenderCreateInternal(
                CMUTIL_GetMem(), name, fpath, logterm, rollpath, pattern);
}

typedef struct CMUTIL_LogSocketAppender {
    CMUTIL_LogAppenderBase base;
    CMBool         isrunning;
    int                 dummy_padder;
    CMUTIL_ServerSocket *listener;
    CMUTIL_Thread       *acceptor;
    CMUTIL_Array        *clients;
} CMUTIL_LogSocketAppender;

CMUTIL_STATIC void CMUTIL_LogSocketAppenderWrite(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    uint32_t i;
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    CMSync(iap->base.mutex, {
        for (i = 0; i < (uint32_t)CMCall(iap->clients, GetSize); i++) {
            CMUTIL_Socket *cli = (CMUTIL_Socket*)
                    CMCall(iap->clients, GetAt, i);
            const char *msg = CMCall(logmsg, GetCString);
            size_t size = CMCall(logmsg, GetSize);
            if (CMCall(cli, Write, msg, (uint32_t)size, 1000) != CMSocketOk) {
                CMCall(iap->clients, RemoveAt, i);
                i--;
                CMCall(cli, Close);
            }
        }
    });
    CMUTIL_UNUSED(logtm);
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderFlush(CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    CMSync(iap->base.mutex, {
        CMCall(iap->base.flush_buffer, MoveAll, iap->base.buffer);
        CMUTIL_List *list = iap->base.flush_buffer;
        if (CMCall(list, GetSize) > 0) {
            CMUTIL_String *logbuf =
                    CMUTIL_StringCreateInternal(iap->base.memst, 1024, NULL);
            while (CMCall(list, GetSize) > 0) {
                CMUTIL_LogAppderAsyncItem *item = (CMUTIL_LogAppderAsyncItem*)
                        CMCall(list, RemoveFront);
                CMCall(logbuf, AddAnother, item->log);
                CMUTIL_LogAppderAsyncItemDestroy(item);
            }
            CMUTIL_LogSocketAppenderWrite(
                        (CMUTIL_LogAppenderBase*)appender, logbuf, NULL);
            CMCall(logbuf, Destroy);
        }
    });
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderDestroy(CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    if (iap) {
        iap->isrunning = CMFalse;
        CMUTIL_Mem *memst =
                CMUTIL_LogAppenderBaseClean((CMUTIL_LogAppenderBase*)iap);
        CMCall(iap->listener, Close);
        CMCall(iap->acceptor, Join);
        CMCall(iap->clients, Destroy);
        memst->Free(iap);
    }
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderCliDestroyer(void *dat)
{
    CMCall((CMUTIL_Socket*)dat, Close);
}

CMUTIL_STATIC void *CMUTIL_LogSocketAppenderAcceptor(void *data)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)data;
    while (iap->isrunning) {
        CMUTIL_Socket *cli = NULL;
        const CMSocketResult sr = CMCall(iap->listener, Accept, &cli, 1000);
        if (sr == CMSocketOk) {
            CMSync(iap->base.mutex, CMCall(iap->clients, Add, cli, NULL););
        }
    }
    return NULL;
}

CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name, const char *accept_host,
        int listen_port, const char *pattern)
{
    char namebuf[256];
    CMUTIL_LogSocketAppender *res =
            memst->Alloc(sizeof(CMUTIL_LogSocketAppender));
    memset(res, 0x0, sizeof(CMUTIL_LogSocketAppender));
    res->base.memst = memst;
    if (!CMUTIL_LogAppenderBaseInit(
                (CMUTIL_LogAppenderBase*)res, pattern, name,
                CMUTIL_LogSocketAppenderWrite,
                CMUTIL_LogSocketAppenderFlush,
                CMUTIL_LogSocketAppenderDestroy)) {
        CMCall((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }
    res->listener = CMUTIL_ServerSocketCreateInternal(
                memst, accept_host, listen_port, 256, CMTrue);
    if (!res->listener) goto FAILED;
    res->clients = CMUTIL_ArrayCreateInternal(
                memst, 10, NULL,
                CMUTIL_LogSocketAppenderCliDestroyer, CMFalse);
    sprintf(namebuf, "LogAppender(%s)-SocketAcceptor", name);
    res->isrunning = CMTrue;
    res->acceptor = CMUTIL_ThreadCreateInternal(
                memst, CMUTIL_LogSocketAppenderAcceptor, res,
                namebuf);
    CMCall(res->acceptor, Start);

    return (CMUTIL_LogAppender*)res;
FAILED:
    CMCall((CMUTIL_LogAppender*)res, Destroy);
    return NULL;
}

CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreate(
    const char *name,
    const char *accept_host,
    int listen_port,
    const char *pattern)
{
    return CMUTIL_LogSocketAppenderCreateInternal(
                CMUTIL_GetMem(), name, accept_host, listen_port, pattern);
}


typedef struct CMUTIL_LogSystem_Internal
{
    CMUTIL_LogSystem    base;
    CMUTIL_Map          *appenders;
    CMUTIL_Array        *cloggers;
    CMUTIL_Array        *loggers;
    CMUTIL_Array        *levelloggers;
    CMUTIL_Mem          *memst;
    CMUTIL_Mutex        *mutex;
} CMUTIL_LogSystem_Internal;

typedef struct CMUTIL_ConfLogger_Internal CMUTIL_ConfLogger_Internal;
struct CMUTIL_ConfLogger_Internal
{
    CMUTIL_ConfLogger   base;
    char                *name;
    CMLogLevel          level;
    CMBool              additivity;
    CMUTIL_Array        *lvlapndrs;
    const CMUTIL_LogSystem_Internal	*lsys;
    void (*Destroy)(CMUTIL_ConfLogger_Internal *icl);
    CMBool (*Log)(CMUTIL_ConfLogger_Internal *cli,
                       CMUTIL_Logger_Internal *li,
                       CMLogLevel level,
                       CMUTIL_StringArray *stack,
                       const char *file,
                       int line,
                       CMUTIL_String *logmsg);
};

CMUTIL_STATIC void CMUTIL_LogSystemAddAppender(
        const CMUTIL_LogSystem *lsys, CMUTIL_LogAppender *appender)
{
    const CMUTIL_LogSystem_Internal *ilsys =
            (const CMUTIL_LogSystem_Internal*)lsys;
    const char *name = CMCall(appender, GetName);
    CMSync(ilsys->mutex, {
        if (CMCall(ilsys->appenders, Get, name) == NULL)
            CMCall(ilsys->appenders, Put, name, appender, NULL);
    });
}

CMUTIL_STATIC void CMUTIL_DoubleArrayDestroyer(void *data)
{
    CMUTIL_Array *arr = (CMUTIL_Array*)data;
    CMCall(arr, Destroy);
}

CMUTIL_STATIC void CMUTIL_ConfLoggerDestroy(CMUTIL_ConfLogger_Internal *icl)
{
    if (icl) {
        if (icl->name)
            icl->lsys->memst->Free(icl->name);
        if (icl->lvlapndrs)
            CMCall(icl->lvlapndrs, Destroy);
        icl->lsys->memst->Free(icl);
    }
}

CMUTIL_STATIC CMBool CMUTIL_ConfLoggerLog(
        CMUTIL_ConfLogger_Internal *cli,
        CMUTIL_Logger_Internal *li,
        CMLogLevel level,
        CMUTIL_StringArray *stack,
        const char *file,
        int line,
        CMUTIL_String *logmsg)
{
    CMUTIL_Array *apndrs = CMCall(cli->lvlapndrs, GetAt, level);
    if (CMCall(apndrs, GetSize) > 0) {
        uint32_t i;
        for (i=0; i<CMCall(apndrs, GetSize); i++) {
            CMUTIL_LogAppender *ap =
                    (CMUTIL_LogAppender*)CMCall(apndrs, GetAt, i);
            CMCall(ap, Append, &li->base, level, stack,
                        file, line, logmsg);
        }
        return CMTrue;
    } else {
        return CMFalse;
    }
}

CMUTIL_STATIC int CMUTIL_LogAppenderComparator(const void *a, const void *b)
{
    const CMUTIL_LogAppender *la = (const CMUTIL_LogAppender*)a;
    const CMUTIL_LogAppender *lb = (const CMUTIL_LogAppender*)b;
    return strcmp(CMCall(la, GetName), CMCall(lb, GetName));
}

CMUTIL_STATIC void CMUTIL_ConfLoggerAddAppender(
        const CMUTIL_ConfLogger *clogger,
        CMUTIL_LogAppender *appender,
        CMLogLevel level)
{
    const CMUTIL_ConfLogger_Internal *cli =
            (const CMUTIL_ConfLogger_Internal*)clogger;
    CMCall((CMUTIL_LogSystem*)cli->lsys, AddAppender, appender);
    for ( ;level <= CMLogLevel_Fatal; level++) {
        CMUTIL_Array *lapndrs = (CMUTIL_Array*)CMCall(
                    cli->lvlapndrs, GetAt, level);
        CMCall(lapndrs, Add, appender, NULL);
    }
}

CMUTIL_STATIC CMUTIL_ConfLogger *CMUTIL_LogSystemCreateLogger(
        const CMUTIL_LogSystem *lsys, const char *name, CMLogLevel level,
        CMBool additivity)
{
    uint32_t i, lvllen = CMLogLevel_Fatal + 1;
    const CMUTIL_LogSystem_Internal *ilsys =
            (const CMUTIL_LogSystem_Internal*)lsys;
    CMUTIL_ConfLogger_Internal *res =
            ilsys->memst->Alloc(sizeof(CMUTIL_ConfLogger_Internal));
    memset(res, 0x0, sizeof(CMUTIL_ConfLogger_Internal));
    if (!name) name = "";
    res->name = ilsys->memst->Strdup(name);
    res->level = level;
    res->additivity = additivity;
    res->lsys = ilsys;
    res->lvlapndrs = CMUTIL_ArrayCreateInternal(
                ilsys->memst, lvllen, NULL,
                CMUTIL_DoubleArrayDestroyer, CMFalse);
    for (i=0; i<lvllen; i++) {
        CMCall(res->lvlapndrs, Add, CMUTIL_ArrayCreateInternal(
                   ilsys->memst, 3, CMUTIL_LogAppenderComparator,
                   NULL, CMTrue), NULL);
    }
    CMCall(ilsys->cloggers, Add, res, NULL);
    for (i=level; i<lvllen; i++) {
        CMUTIL_Array* lvllog = CMCall(ilsys->levelloggers, GetAt, i);
        CMCall(lvllog, Add, res, NULL);
    }
    res->base.AddAppender = CMUTIL_ConfLoggerAddAppender;
    res->Log = CMUTIL_ConfLoggerLog;
    res->Destroy = CMUTIL_ConfLoggerDestroy;
    return (CMUTIL_ConfLogger*)res;
}

CMUTIL_STATIC int CMUTIL_LoggerRefComparator(const void *a, const void *b)
{
    const CMUTIL_ConfLogger_Internal *ca = (const CMUTIL_ConfLogger_Internal*)a;
    const CMUTIL_ConfLogger_Internal *cb = (const CMUTIL_ConfLogger_Internal*)b;
    const size_t sa = strlen(ca->name);
    const size_t sb = strlen(cb->name);
    // longer name is first.
    if (sa < sb) return 1;
    if (sa > sb) return -1;
    return 0;
}

CMUTIL_STATIC void CMUTIL_LoggerLogEx(
        CMUTIL_Logger *logger, CMLogLevel level,
        const char *file, int line, CMBool printStack,
        const char *fmt, ...)
{
    uint32_t i;
    CMUTIL_Logger_Internal *il = (CMUTIL_Logger_Internal*)logger;
    CMUTIL_String *logmsg;
    CMUTIL_StringArray *stack = NULL;
    CMUTIL_StackWalker *sw = NULL;
    va_list args;
    if (level < il->minlevel) return;
    logmsg = CMUTIL_StringCreateInternal(il->memst, 128, NULL);
    va_start(args, fmt);
    CMCall(logmsg, AddVPrint, fmt, args);
    va_end(args);
    // print stack
    if (printStack) {
        sw = CMUTIL_StackWalkerCreateInternal(il->memst);
        stack = CMCall(sw, GetStack, 1);
    }
    for (i=0; i<CMCall(il->logrefs, GetSize); i++) {
        CMUTIL_ConfLogger_Internal *cli = (CMUTIL_ConfLogger_Internal*)
                CMCall(il->logrefs, GetAt, i);
        if (cli->level <= level) {
            if (CMCall(cli, Log, il, level, stack, file, line, logmsg))
                if (!cli->additivity) break;
        }
    }
    CMCall(logmsg, Destroy);
    if (sw) CMCall(sw, Destroy);
    if (stack) CMCall(stack, Destroy);
}

CMUTIL_STATIC CMBool CMUTIL_LoggerIsEnabled(
    CMUTIL_Logger *logger, CMLogLevel level)
{
    CMUTIL_Logger_Internal *il = (CMUTIL_Logger_Internal*)logger;
    if (level < il->minlevel) return CMFalse;
    return CMTrue;
}

CMUTIL_STATIC void CMUTIL_LoggerDestroy(CMUTIL_Logger_Internal *logger)
{
    CMUTIL_Logger_Internal *il = logger;
    if (il) {
        if (il->fullname)
            CMCall(il->fullname, Destroy);
        if (il->namepath)
            CMCall(il->namepath, Destroy);
        if (il->logrefs)
            CMCall(il->logrefs, Destroy);
        il->memst->Free(il);
    }
}

CMUTIL_STATIC CMUTIL_Logger *CMUTIL_LogSystemGetLogger(
        const CMUTIL_LogSystem *lsys, const char *name)
{
    const CMUTIL_LogSystem_Internal *ilsys =
            (const CMUTIL_LogSystem_Internal*)lsys;
    CMUTIL_Logger_Internal q;
    CMUTIL_Logger_Internal* res = NULL;
    q.fullname = CMUTIL_StringCreateInternal(ilsys->memst, 20, name);

    CMCall(ilsys->mutex, Lock);
    res = (CMUTIL_Logger_Internal*)CMCall(ilsys->loggers, Find, &q, NULL);
    if (res == NULL) {
        uint32_t i;
        // create logger
        res = ilsys->memst->Alloc(sizeof(CMUTIL_Logger_Internal));
        memset(res, 0x0, sizeof(CMUTIL_Logger_Internal));
        res->memst = ilsys->memst;
        res->fullname = q.fullname;
        res->namepath = CMUTIL_StringSplitInternal(ilsys->memst, name, ".");
        res->logrefs = CMUTIL_ArrayCreateInternal(
                    ilsys->memst, 3, CMUTIL_LoggerRefComparator, NULL, CMTrue);
        res->minlevel = CMLogLevel_Fatal;
        for (i=0; i<CMCall(ilsys->cloggers, GetSize); i++) {
            CMUTIL_ConfLogger_Internal *cl = (CMUTIL_ConfLogger_Internal*)
                    CMCall(ilsys->cloggers, GetAt, i);
            // adding root logger and parent loggers
            if (strlen(cl->name) == 0 || strstr(name, cl->name) == 0) {
                CMCall(res->logrefs, Add, cl, NULL);
                if (res->minlevel > cl->level)
                    res->minlevel = cl->level;
            }
        }
        // logex and destroyer
        res->base.LogEx = CMUTIL_LoggerLogEx;
        res->base.IsEnabled = CMUTIL_LoggerIsEnabled;
        res->Destroy = CMUTIL_LoggerDestroy;
        CMCall(ilsys->loggers, Add, res, NULL);
    } else {
        CMCall(q.fullname, Destroy);
    }
    CMCall(ilsys->mutex, Unlock);
    return (CMUTIL_Logger*)res;
}

CMUTIL_STATIC void CMUTIL_LogAppenderDestroyer(void *data)
{
    CMUTIL_LogAppender *ap = (CMUTIL_LogAppender*)data;
    CMCall(ap, Destroy);
}

CMUTIL_STATIC int CMUTIL_ConfLoggerComparator(const void *a, const void *b)
{
    const CMUTIL_ConfLogger_Internal *acli = (const CMUTIL_ConfLogger_Internal*)a;
    const CMUTIL_ConfLogger_Internal *bcli = (const CMUTIL_ConfLogger_Internal*)b;
    return strcmp(acli->name, bcli->name);
}

CMUTIL_STATIC void CMUTIL_ConfLoggerDestroyer(void *data)
{
    CMUTIL_ConfLogger_Internal *ap = (CMUTIL_ConfLogger_Internal*)data;
    CMCall(ap, Destroy);
}

CMUTIL_STATIC int CMUTIL_LoggerComparator(const void *a, const void *b)
{
    const CMUTIL_Logger_Internal *al = (const CMUTIL_Logger_Internal*)a;
    const CMUTIL_Logger_Internal *bl = (const CMUTIL_Logger_Internal*)b;
    return strcmp(
                CMCall(al->fullname, GetCString),
                CMCall(bl->fullname, GetCString));
}

CMUTIL_STATIC void CMUTIL_LoggerDestroyer(void *data)
{
    CMUTIL_Logger_Internal *ap = (CMUTIL_Logger_Internal*)data;
    CMCall(ap, Destroy);
}

CMUTIL_STATIC void CMUTIL_LogSystemDestroy(CMUTIL_LogSystem *lsys)
{
    CMUTIL_LogSystem_Internal *ilsys = (CMUTIL_LogSystem_Internal*)lsys;
    if (ilsys) {
        if (ilsys->appenders) CMCall(ilsys->appenders, Destroy);
        if (ilsys->cloggers) CMCall(ilsys->cloggers, Destroy);
        if (ilsys->levelloggers) CMCall(ilsys->levelloggers, Destroy);
        if (ilsys->loggers) CMCall(ilsys->loggers, Destroy);
        if (ilsys->mutex) CMCall(ilsys->mutex, Destroy);
        ilsys->memst->Free(ilsys);
    }
}

CMBool CMUTIL_LogIsEnabled(CMUTIL_Logger *logger, CMLogLevel level)
{
    if (logger)
        return CMCall(logger, IsEnabled, level);
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemUpdateEnv(CMUTIL_LogSystem *lsys)
{
    CMUTIL_LogSystem_Internal *res = (CMUTIL_LogSystem_Internal*)lsys;
    CMBool ret = CMTrue;
    if (res->appenders) {
        CMUTIL_StringArray *keys = CMCall(res->appenders, GetKeys);
        for (int i = 0; i < CMCall(keys, GetSize); i++) {
            const char *key = CMCall(keys, GetCString, i);
            CMUTIL_LogAppenderBase *iap = CMCall(res->appenders, Get, key);
            ret = ret &&CMUTIL_LogAppenderPatternRebuild(iap);
        }
        CMCall(keys, Destroy);
    }
    return ret;
}

CMUTIL_LogSystem *CMUTIL_LogSystemCreateInternal(CMUTIL_Mem *memst)
{
    int i;
    CMUTIL_LogSystem_Internal *res =
            memst->Alloc(sizeof(CMUTIL_LogSystem_Internal));
    memset(res, 0x0, sizeof(CMUTIL_LogSystem_Internal));
    res->base.AddAppender = CMUTIL_LogSystemAddAppender;
    res->base.CreateLogger = CMUTIL_LogSystemCreateLogger;
    res->base.GetLogger = CMUTIL_LogSystemGetLogger;
    res->base.Destroy = CMUTIL_LogSystemDestroy;
    res->base.UpdateEnv = CMUTIL_LogSystemUpdateEnv;
    res->memst = memst;
    res->mutex = CMUTIL_MutexCreateInternal(memst);
    res->appenders = CMUTIL_MapCreateInternal(
                memst, 256, CMFalse, CMUTIL_LogAppenderDestroyer);
    res->cloggers = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_ConfLoggerComparator,
                CMUTIL_ConfLoggerDestroyer, CMTrue);
    res->levelloggers = CMUTIL_ArrayCreateInternal(
                memst, CMLogLevel_Fatal+1, NULL,
                CMUTIL_DoubleArrayDestroyer, CMFalse);
    for (i=0; i<(CMLogLevel_Fatal+1); i++)
        CMCall(res->levelloggers, Add, CMUTIL_ArrayCreateInternal(
                        memst, 3, CMUTIL_ConfLoggerComparator, NULL, CMTrue),
                        NULL);
    res->loggers = CMUTIL_ArrayCreateInternal(
                memst, 50, CMUTIL_LoggerComparator,
                CMUTIL_LoggerDestroyer, CMTrue);

    return (CMUTIL_LogSystem*)res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemCreate()
{
    return CMUTIL_LogSystemCreateInternal(CMUTIL_GetMem());
}

void CMUTIL_LogSystemSet(CMUTIL_LogSystem *lsys)
{
    CMCall(g_cmutil_logsystem_mutex, Lock);
    if (__cmutil_logsystem) {
        CMCall(__cmutil_logsystem, Destroy);
        __cmutil_logsystem = NULL;
    }
    __cmutil_logsystem = lsys;
    CMCall(g_cmutil_logsystem_mutex, Unlock);
}

CMUTIL_STATIC void CMUTIL_LogConfigClean(CMUTIL_Json *json)
{
    uint32_t i;
    switch (CMCall(json, GetType)) {
    case CMJsonTypeObject: {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMUTIL_StringArray *keys = CMCall(jobj, GetKeys);
        for (i=0; i<CMCall(keys, GetSize); i++) {
            const CMUTIL_String *str = CMCall(keys, GetAt, i);
            const char *key = CMCall(str, GetCString);
            CMUTIL_Json *item = CMCall(jobj, Remove, key);
            // change key to lowercase
            CMUTIL_String *nstr = CMCall(str, ToLower);
            // change item recursively.
            CMUTIL_LogConfigClean(item);
            // 'key' is changed to lowercase already.
            // so just put with it.
            key = CMCall(nstr, GetCString);
            CMCall(jobj, Put, key, item);
            CMCall(nstr, Destroy);
        }
        CMCall(keys, Destroy);
        break;
    }
    case CMJsonTypeArray: {
        CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)json;
        for (i=0; i<CMCall(jarr, GetSize); i++)
            // change item recursively.
            CMUTIL_LogConfigClean(CMCall(jarr, Get, i));
        break;
    }
    default: return;
    }
}

#define CMUITL_LOG_PATTERN_DEFAULT	"%d %P-[%-10.10t] (%-15.15F:%04L) [%-5p] %c - %m%ex%n"

static const char *g_cmutil_log_term_format[] = {
    "%s.%%Y",
    "%s.%%Y-%%m",
    "%s.%%Y-%%m-%%d",
    "%s.%%Y-%%m-%%d_%%H",
    "%s.%%Y-%%m-%%d_%%H%%M"
};

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureDefault(CMUTIL_Mem *memst)
{
    CMCall(g_cmutil_logsystem_mutex, Lock);
    // create default logsystem
    CMUTIL_LogSystem *res = CMUTIL_LogSystemCreateInternal(memst);
    const CMUTIL_ConfLogger* root_logger =
        CMCall(res, CreateLogger, NULL, CMLogLevel_Debug, CMTrue);
    CMUTIL_LogAppender *conapndr = CMUTIL_LogConsoleAppenderCreateInternal(
                memst, "__CONSOL", CMUITL_LOG_PATTERN_DEFAULT,
                CMFalse);
    //CMCall(conapndr, SetAsync, 64);
    CMCall(res, AddAppender, conapndr);
    CMCall(root_logger, AddAppender, conapndr, CMLogLevel_Debug);
    __cmutil_logsystem = (CMUTIL_LogSystem*)res;
    CMCall(g_cmutil_logsystem_mutex, Unlock);

    printf("logging system not initialized or invalid configuration. "
              "using default configuraiton."S_CRLF);
    return res;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessOneAppender(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL, *spattern = NULL;
    CMUTIL_LogAppender *appender = NULL;
    CMBool async = CMFalse;
    CMBool use_stderr = CMFalse;
    int64_t asyncBufsz = 256;

    if (CMCall(json, GetType) != CMJsonTypeObject) {
        printf("invalid configuration structure.\n");
        return CMFalse;
    }

    if (CMCall(jobj, Get, "name")) {
        sname = CMCall(jobj, GetCString, "name");
    } else {
        printf("name must be specified in any appender.\n");
        return CMFalse;
    }
    if (CMCall(jobj, Get, "pattern")) {
        spattern = CMCall(jobj, GetCString, "pattern");
    } else {
        spattern = CMUITL_LOG_PATTERN_DEFAULT;
    }
    if (CMCall(jobj, Get, "async")) {
        async = CMCall(jobj, GetBoolean, "async");
    }
    if (CMCall(jobj, Get, "useStderr")) {
        use_stderr = CMCall(jobj, GetBoolean, "useStderr");
    }
    if (CMCall(jobj, Get, "asyncbuffersize")) {
        asyncBufsz = CMCall(jobj, GetLong, "asyncbuffersize");
        if (asyncBufsz < 10) {
            printf("'asyncBufferSize' too small, must be greater than 10\n");
            return CMFalse;
        }
    }

    if (strcmp("console", stype) == 0) {
        appender = CMUTIL_LogConsoleAppenderCreateInternal(
                    lsys->memst, sname, spattern, use_stderr);
    } else if (strcmp("file", stype) == 0) {
        const char *filename = NULL;
        if (CMCall(jobj, Get, "filename")) {
            filename = CMCall(jobj, GetCString, "filename");
        } else {
            printf("fileName must be specified in File type appender.\n");
            return CMFalse;
        }
        appender = CMUTIL_LogFileAppenderCreateInternal(
                    lsys->memst, sname, filename, spattern);
    } else if (strcmp("rollingfile", stype) == 0) {
        char deffpattern[1024];
        const char *filename = NULL, *filepattern = NULL;
        CMLogTerm rollterm = CMLogTerm_Date;
        if (CMCall(jobj, Get, "filename")) {
            filename = CMCall(jobj, GetCString, "filename");
        } else {
            printf("fileName must be specified in RollingFile "
                   "type appender.\n");
            return CMFalse;
        }
        if (CMCall(jobj, Get, "rollterm")) {
            const char *sterm = CMCall(jobj, GetCString, "rollterm");
            if (strcasecmp("year", sterm) == 0) {
                rollterm = CMLogTerm_Year;
            } else if (strcasecmp("month", sterm) == 0) {
                rollterm = CMLogTerm_Month;
                // date term skipped(already set)
            } else if (strcasecmp("hour", sterm) == 0) {
                rollterm = CMLogTerm_Hour;
            } else if (strcasecmp("minute", sterm) == 0) {
                rollterm = CMLogTerm_Minute;
            }
        }
        if (CMCall(jobj, Get, "filepattern")) {
            filepattern = CMCall(jobj, GetCString, "filepattern");
        } else {
            sprintf(deffpattern, g_cmutil_log_term_format[rollterm], filename);
            filepattern = deffpattern;
        }
        appender = CMUTIL_LogRollingFileAppenderCreateInternal(
                    lsys->memst, sname, filename, rollterm,
                    filepattern, spattern);
    } else if (strcmp("socket", stype) == 0) {
        const char *shost = "INADDR_ANY";
        int port;
        if (CMCall(jobj, Get, "listenport")) {
            port = (int)CMCall(jobj, GetLong, "listenport");
        } else {
            printf("listenPort must be specified in Socket type appender.\n");
            return CMFalse;
        }
        if (CMCall(jobj, Get, "accepthost")) {
            shost = CMCall(jobj, GetCString, "accepthost");
        }
        appender = CMUTIL_LogSocketAppenderCreateInternal(
                    lsys->memst, sname, shost, port, spattern);
    }

    if (appender) {
        if (async) CMCall(appender, SetAsync, (size_t)asyncBufsz);
        CMCall(&(lsys->base), AddAppender, appender);
        return CMTrue;
    } else {
        printf("unknown appender type '%s'.\n", stype);
        return CMFalse;
    }
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemAddAppenderToLogger(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_ConfLogger *clogger,
        const char *apndr, CMLogLevel level)
{
    CMUTIL_LogAppender *appender = CMCall(lsys->appenders, Get, apndr);
    if (!appender) {
        printf("appender '%s' not found in system.\n", apndr);
        return CMFalse;
    }
    CMCall(clogger, AddAppender, appender, level);
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogStringToLevel(
        const char *slevel, CMLogLevel *level)
{
    if (strcasecmp(slevel, "trace") == 0)
        *level = CMLogLevel_Trace;
    else if (strcasecmp(slevel, "debug") == 0)
        *level = CMLogLevel_Debug;
    else if (strcasecmp(slevel, "info") == 0)
        *level = CMLogLevel_Info;
    else if (strcasecmp(slevel, "warn") == 0)
        *level = CMLogLevel_Warn;
    else if (strcasecmp(slevel, "error") == 0)
        *level = CMLogLevel_Error;
    else if (strcasecmp(slevel, "fatal") == 0)
        *level = CMLogLevel_Fatal;
    else
        return CMFalse;
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessAppenderRef(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_ConfLogger *clogger,
        CMUTIL_Json *json, CMLogLevel level)
{
    if (CMCall(json, GetType) == CMJsonTypeValue) {
        CMUTIL_JsonValue *jval = (CMUTIL_JsonValue*)json;
        const char *sval = CMCall(jval, GetCString);
        return CMUTIL_LogSystemAddAppenderToLogger(
                    lsys, clogger, sval, level);
    }
    if (CMCall(json, GetType) == CMJsonTypeObject) {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMLogLevel alevel = level;
        const char *ref = NULL;
        if (CMCall(jobj, Get, "ref")) {
            ref = CMCall(jobj, GetCString, "ref");
        } else {
            printf("ref must be specified in AppenderRef.\n");
            return CMFalse;
        }
        if (CMCall(jobj, Get, "level")) {
            const char *salevel = CMCall(jobj, GetCString, "level");
            if (!CMUTIL_LogStringToLevel(salevel, &alevel)) {
                printf("invalid level in AppenderRef.\n");
                return CMFalse;
            }
        }
        return CMUTIL_LogSystemAddAppenderToLogger(
                    lsys, clogger, ref, alevel);
    }
    printf("invalid type of AppenderRef\n");
    return CMFalse;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessOneLogger(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMBool res = CMTrue;
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL;
    CMUTIL_ConfLogger *clogger = NULL;
    CMBool additivity = CMTrue;
    CMLogLevel level = CMLogLevel_Info;
    CMUTIL_Json *apref = NULL;

    if (CMCall(json, GetType) != CMJsonTypeObject) {
        printf("invalid configuration structure.\n");
        return CMFalse;
    }

    if (CMCall(jobj, Get, "name")) {
        const CMUTIL_String *name = CMCall(jobj, GetString, "name");
        CMUTIL_String *cname = CMCall(name, Clone);
        CMCall(cname, SelfTrim);
        sname = CMCall(cname, GetCString);
        if (*sname == 0x0 && strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            CMCall(cname, Destroy);
            return CMFalse;
        }
        CMCall(cname, Destroy);
    } else {
        if (strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            return CMFalse;
        }
    }
    if (strcmp(stype, "root") == 0) sname = "";
    if (CMCall(jobj, Get, "level")) {
        const char *slevel = CMCall(jobj, GetCString, "level");
        if (!CMUTIL_LogStringToLevel(slevel, &level)) {
            printf("invalid log level in logger.\n");
            return CMFalse;
        }
    } else {
        printf("level must be specified in logger.\n");
        return CMFalse;
    }
    if (CMCall(jobj, Get, "additivity")) {
        additivity = CMCall(jobj, GetBoolean, "additivity");
    }

    clogger = CMCall(
                &(lsys->base), CreateLogger, sname, level, additivity);
    apref = CMCall(jobj, Get, "appenderref");
    if (apref) {
        switch (CMCall(apref, GetType)) {
        case CMJsonTypeArray: {
            CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)apref;
            uint32_t i;
            for (i=0; i<CMCall(jarr, GetSize); i++) {
                CMUTIL_Json *item = CMCall(jarr, Get, i);
                if (CMCall(item, GetType) == CMJsonTypeArray) {
                    printf("invalid appender reference in logger.\n");
                    return CMFalse;
                }
                res = CMUTIL_LogSystemProcessAppenderRef(
                            lsys, clogger, item, level);
                if (!res) {
                    printf("invalid appender reference in logger.\n");
                    return res;
                }
            }
            break;
        }
        case CMJsonTypeValue:
            return CMUTIL_LogSystemProcessAppenderRef(
                        lsys, clogger, apref, level);
        default:
            break;
        }
    }
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessItems(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_Json *json,
        CMBool(*oneprocf)(
            CMUTIL_LogSystem_Internal*, const char *, CMUTIL_Json*))
{
    CMBool res = CMFalse;
    uint32_t i;
    CMUTIL_JsonArray *jarr = NULL;
    if (CMCall(json, GetType) != CMJsonTypeArray) {
        printf("invalid configuration structure.\n");
        goto ENDPOINT;
    }
    jarr = (CMUTIL_JsonArray*)json;
    for (i=0; i<CMCall(jarr, GetSize); i++) {
        CMUTIL_Json *item = CMCall(jarr, Get, i);
        CMUTIL_JsonObject *oitem = NULL;
        const CMUTIL_String *type;
        CMUTIL_String *ctype = NULL;
        const char *stype = NULL;
        if (CMCall(item, GetType) != CMJsonTypeObject) {
            printf("invalid configuration structure.\n");
            goto ENDPOINT;
        }
        oitem = (CMUTIL_JsonObject*)item;
        type = CMCall(oitem, GetString, "type");
        ctype = CMCall(type, Clone);
        CMCall(ctype, SelfToLower);
        stype = CMCall(ctype, GetCString);

        if (!oneprocf(lsys, stype, item)) {
            printf("processing one item failed.\n");
            CMCall(ctype, Destroy);
            goto ENDPOINT;
        }
        CMCall(ctype, Destroy);
    }
    res = CMTrue;
ENDPOINT:
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureInternal(
        CMUTIL_Mem *memst, CMUTIL_Json *json)
{
    CMUTIL_LogSystem *lsys = NULL;
    CMUTIL_JsonObject *config = (CMUTIL_JsonObject*)json;
    CMUTIL_Json *appenders, *loggers;
    CMBool succeeded = CMFalse;

    // all keys to lowercase for case-insensitive.
    CMUTIL_LogConfigClean(json);
    if (CMCall(config, Get, "configuration"))
        config = (CMUTIL_JsonObject*)CMCall(config, Get, "configuration");
    appenders = CMCall(config, Get, "appenders");
    loggers = CMCall(config, Get, "loggers");

    if (appenders && loggers) {
        CMCall(g_cmutil_logsystem_mutex, Lock);
        lsys = CMUTIL_LogSystemCreateInternal(memst);
        succeeded = CMUTIL_LogSystemProcessItems(
                    (CMUTIL_LogSystem_Internal*)lsys, appenders,
                    CMUTIL_LogSystemProcessOneAppender)
                & CMUTIL_LogSystemProcessItems(
                    (CMUTIL_LogSystem_Internal*)lsys, loggers,
                    CMUTIL_LogSystemProcessOneLogger);
        if (succeeded) {
            __cmutil_logsystem = (CMUTIL_LogSystem*)lsys;
        } else {
            if (lsys) CMCall(lsys, Destroy);
        }
        CMCall(g_cmutil_logsystem_mutex, Unlock);
    }
    if (!succeeded)
        lsys = CMUTIL_LogSystemConfigureDefault(memst);
    return lsys;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJsonInternal(
        CMUTIL_Mem *memst, const char *jsonfile)
{
    CMUTIL_LogSystem *res = NULL;
    CMUTIL_File *f = CMUTIL_FileCreateInternal(memst, jsonfile);
    CMUTIL_String *jsonstr = CMCall(f, GetContents);
    if (jsonstr) {
        CMUTIL_Json *json = CMUTIL_JsonParseInternal(memst, jsonstr, CMTrue);
        if (json) {
            res = CMUTIL_LogSystemConfigureInternal(memst, json);
            CMCall(json, Destroy);
        }
        CMCall(jsonstr, Destroy);
    }

    if (res == NULL) {
        res = CMUTIL_LogSystemConfigureDefault(memst);
    }

    if (f)
        CMCall(f, Destroy);
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(const char *jsonfile)
{
    return CMUTIL_LogSystemConfigureFomJsonInternal(
                CMUTIL_GetMem(), jsonfile);
}

static CMBool g_logsystem_initialized = CMFalse;

CMUTIL_LogSystem *CMUTIL_LogSystemGetInternal(CMUTIL_Mem *memst)
{
    if (__cmutil_logsystem) return __cmutil_logsystem;

    CMBool initialize = CMFalse;
    CMCall(g_cmutil_logsystem_mutex, Lock);
    if (!g_logsystem_initialized) {
        initialize = CMTrue;
        g_logsystem_initialized = CMTrue;
    }
    CMCall(g_cmutil_logsystem_mutex, Unlock);

    if (__cmutil_logsystem == NULL && initialize) {
        // try to read cmutil_log.json
        char *conf = getenv("CMUTIL_LOG_CONFIG");
        if (!conf) conf = CMUTIL_LOG_CONFIG_DEFAULT;
        __cmutil_logsystem = CMUTIL_LogSystemConfigureFomJsonInternal(memst, conf);
        return __cmutil_logsystem;
    }
    return NULL;
}

CMUTIL_LogSystem *CMUTIL_LogSystemGet()
{
    return CMUTIL_LogSystemGetInternal(CMUTIL_GetMem());
}

void CMUTIL_LogFallback(const CMLogLevel level,
        const char *file, const int line, const char *fmt, ...)
{
    char fmtbuf[1024] = {0,};
    va_list args;
    const char *levelstr = NULL;

    switch (level) {
        case CMLogLevel_Trace: levelstr = "TRACE"; break;
        case CMLogLevel_Debug: levelstr = "DEBUG"; break;
        case CMLogLevel_Info:  levelstr = "INFO "; break;
        case CMLogLevel_Warn:  levelstr = "WARN "; break;
        case CMLogLevel_Error: levelstr = "ERROR"; break;
        case CMLogLevel_Fatal: levelstr = "FATAL"; break;
        default: levelstr = "UNKN "; break;
    }

    va_start(args, fmt);
    sprintf(fmtbuf, "[%s] %15s(%04d) - %s\n", levelstr, file, line, fmt);
    vprintf(fmtbuf, args);
    va_end(args);
}