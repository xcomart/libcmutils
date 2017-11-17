/*
                      GNU General Public License
    libcmutils is a bunch of commonly used utility functions for multiplatform.
    Copyright (C) 2016 Dennis Soungjin Park <xcomart@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "functions.h"
#include <time.h>
#include <stdarg.h>
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

    // number of enumeration indicator
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

    // number of enumeration indicator
    LogType_Length
} CMUTIL_LogFormatType;

typedef struct CMUTIL_LogAppenderFormatItem CMUTIL_LogAppenderFormatItem;
struct CMUTIL_LogAppenderFormatItem {
    CMUTIL_LogFormatItemType	type;
    CMBool					padleft;
    uint32_t						precision[10];
    uint32_t						precisioncnt;
    CMBool					hasext;
    size_t						length;
    CMUTIL_String				*data;
    CMUTIL_String				*data2;
    void						*anything;
    CMUTIL_Mem                  *memst;
};

typedef struct CMUTIL_LogAppenderBase CMUTIL_LogAppenderBase;
struct CMUTIL_LogAppenderBase {
    CMUTIL_LogAppender	base;
    CMBool			isasync;
    int                 dummy_padder;
    size_t				buffersz;
    void                (*Write)(CMUTIL_LogAppenderBase *appender,
                                 CMUTIL_String *logmsg,
                                 struct tm *logtm);
    char				*name;
    CMUTIL_List			*pattern;
    CMUTIL_List			*buffer;
    CMUTIL_List			*flush_buffer;
    CMUTIL_Thread		*writer;
    CMUTIL_Mutex		*mutex;
    CMUTIL_Mem          *memst;
};

typedef struct CMUTIL_Logger_Internal CMUTIL_Logger_Internal;
struct CMUTIL_Logger_Internal {
    CMUTIL_Logger				base;
    CMUTIL_LogLevel				minlevel;
    int         				dummy_padder;
    CMUTIL_String				*fullname;
    CMUTIL_StringArray			*namepath;
    CMUTIL_Array				*logrefs;
    CMUTIL_Mem                  *memst;
    void                        (*Destroy)(CMUTIL_Logger_Internal *logger);
};

typedef struct CMUTIL_LogPatternAppendFuncParam {
    CMUTIL_LogAppenderFormatItem	*item;
    CMUTIL_LogLevel					level;
    int								linenum;
    CMUTIL_String					*dest;
    CMUTIL_Logger					*logger;
    CMUTIL_StringArray				*stack;
    struct tm						*logtm;
    long							millis;
    const char						*fname;
    CMUTIL_String					*logmsg;
} CMUTIL_LogPatternAppendFuncParam;

typedef void (*CMUTIL_LogPatternAppendFunc)(
    CMUTIL_LogPatternAppendFuncParam *params);
static CMUTIL_Map *g_cmutil_log_item_map = NULL;
static CMUTIL_LogFormatType g_cmutil_log_type_arr[LogType_Length];
static CMUTIL_Map *g_cmutil_log_datefmt_map = NULL;
static CMUTIL_StringArray *g_cmutil_log_levels;
static CMUTIL_LogPatternAppendFunc
        g_cmutil_log_pattern_funcs[LogFormatItem_Length];
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
    strncat(pdir, fpath, (size_t)(p - fpath));
    CMUTIL_PathCreate(pdir, 0755);
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
    CMUTIL_LogAppenderFormatItem *item,
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
    }
    else {
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
    CMUTIL_String *buf = CMUTIL_StringCreateInternal(ilog->memst, 10, NULL);
    CMUTIL_String *curr = NULL;
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
        uint32_t i, size = params->item->precision[0];
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
    char buf[256];
    CMUTIL_Thread *t = CMUTIL_ThreadSelf();
    unsigned int tid = CMCall(t, GetId);
    const char *name = CMCall(t, GetName);
    uint32_t size = (uint32_t)sprintf(buf, "%s-%u", name, tid);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendFileName(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    uint32_t tsize = (uint32_t)strlen(params->fname), size = 0;
    const char *p = params->fname + (tsize - 1);
    while (p > params->fname && !strchr("/\\", *p)) {
        p--;
        size++;
    }
    if (strchr("/\\", *p)) p++;
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, p, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLineNumber(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    char buf[20];
    uint32_t size = (uint32_t)sprintf(buf, "%d", params->linenum);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLogLevel(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_String *val = CMCall(g_cmutil_log_levels, GetAt, params->level);
    if (params->item->hasext) {
        CMUTIL_Map *lvlmap = (CMUTIL_Map*)params->item->anything;
        CMUTIL_String *key = val;
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
            CMUTIL_String *item = CMCall(params->stack, GetAt, i);
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
        &g_cmutil_log_type_arr[b])

    LOGTYPE_ADD("d"				, LogType_DateTime);
    LOGTYPE_ADD("date"			, LogType_DateTime);
    LOGTYPE_ADD("c"				, LogType_LoggerName);
    LOGTYPE_ADD("logger"		, LogType_LoggerName);
    LOGTYPE_ADD("t"				, LogType_ThreadId);
    LOGTYPE_ADD("tid"			, LogType_ThreadId);
    LOGTYPE_ADD("thread"		, LogType_ThreadId);
    LOGTYPE_ADD("P"				, LogType_ProcessId);
    LOGTYPE_ADD("pid"			, LogType_ProcessId);
    LOGTYPE_ADD("process"		, LogType_ProcessId);
    LOGTYPE_ADD("F"				, LogType_FileName);
    LOGTYPE_ADD("file"			, LogType_FileName);
    LOGTYPE_ADD("L"				, LogType_LineNumber);
    LOGTYPE_ADD("line"			, LogType_LineNumber);
    LOGTYPE_ADD("p"				, LogType_LogLevel);
    LOGTYPE_ADD("level"			, LogType_LogLevel);
    LOGTYPE_ADD("m"				, LogType_Message);
    LOGTYPE_ADD("msg"			, LogType_Message);
    LOGTYPE_ADD("message"		, LogType_Message);
    LOGTYPE_ADD("e"				, LogType_Environment);
    LOGTYPE_ADD("env"			, LogType_Environment);
    LOGTYPE_ADD("environment"	, LogType_Environment);
    LOGTYPE_ADD("ex"			, LogType_Stack);
    LOGTYPE_ADD("s"				, LogType_Stack);
    LOGTYPE_ADD("stack"			, LogType_Stack);
    LOGTYPE_ADD("n"				, LogType_LineSeparator);

    g_cmutil_log_datefmt_map = CMUTIL_MapCreate();
    CMCall(g_cmutil_log_datefmt_map, Put, "DEFAULT", "%F %T.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "ISO8601", "%FT%T.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put,
                "ISO8601_BASIC", "%Y%m%dT%H%M%S.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "ABSOLUTE", "%T.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "DATE", "%d %b %Y %T.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "COMPACT", "%Y%m%d%H%M%S.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "GENERAL", "%Y%m%d %H%M%S.%Q");
    CMCall(g_cmutil_log_datefmt_map, Put, "UNIX", "%s%Q");

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

CMUTIL_STATIC void *CMUTIL_LogAppenderAsyncWriter(void *udata)
{
    CMUTIL_List *temp;
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)udata;
    while (iap->isasync) {
        temp = iap->flush_buffer;
        // swap buffer
        CMCall(iap->mutex, Lock);
        iap->flush_buffer = iap->buffer;
        iap->buffer = temp;
        CMCall(iap->mutex, Unlock);
        CMCall((CMUTIL_LogAppender*)iap, Flush);
        USLEEP(100000);	// sleep 100 milliseconds
    }
    CMCall((CMUTIL_LogAppender*)iap, Flush);
    temp = iap->flush_buffer;
    CMCall(iap->mutex, Lock);
    iap->flush_buffer = iap->buffer;
    iap->buffer = temp;
    CMCall(iap->mutex, Unlock);
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
            sprintf(namebuf, "LogAppender(%s)-AsyncWriter", iap->name);
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

CMUTIL_STATIC void CMUTIL_LogAppderAsyncItemDestroy(
        CMUTIL_LogAppderAsyncItem *item)
{
    if (item) {
        if (item->log) CMCall(item->log, Destroy);
        item->memst->Free(item);
    }
}

CMUTIL_STATIC void CMUTIL_LogAppenderBaseAppend(
    CMUTIL_LogAppender *appender,
    CMUTIL_Logger *logger,
    CMUTIL_LogLevel level,
    CMUTIL_StringArray *stack,
    const char *fname,
    int linenum,
    CMUTIL_String *logmsg)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_Iterator *iter = NULL;
    struct tm currtm;
    struct timeval tv;
    CMUTIL_LogPatternAppendFuncParam param;

    memset(&param, 0x0, sizeof(param));

    gettimeofday(&tv, NULL);
    localtime_r((time_t*)&(tv.tv_sec), &currtm);

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

    CMCall(iap->mutex, Lock);
    if (iap->isasync) {
        if (CMCall(iap->buffer, GetSize) >= iap->buffersz) {
            CMCall(iap->mutex, Unlock);
            CMCall(param.dest, Destroy);
            // TODO: error logging - async buffer full.
            printf("Warning! '%s' async buffer is full.\n", iap->name);
            return;
        }
        else {
            CMCall(iap->buffer, AddTail,
                CMUTIL_LogAppderAsyncItemCreate(
                            iap->memst, param.dest, &currtm));
        }
    }
    else {
        CMCall(iap, Write, param.dest, &currtm);
        CMCall(param.dest, Destroy);
    }
    CMCall(iap->mutex, Unlock);
}

CMUTIL_STATIC CMUTIL_LogAppenderFormatItem *CMUTIL_LogAppenderItemString(
    CMUTIL_Mem *memst, const char *a, size_t length)
{
    CMUTIL_LogAppenderFormatItem *res =
        CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
    res->data = CMUTIL_StringCreate();
    CMCall(res->data, AddNString, a, length);
    return res;
}

CMUTIL_STATIC void CMUTIL_LogPatternLogLevelParse(
    CMUTIL_LogAppenderFormatItem *item, const char *extra)
{
    uint32_t i;
    CMUTIL_Map *map = CMUTIL_MapCreateInternal(
                item->memst, 10, CMFalse,
                CMUTIL_LogAppenderStringDestroyer);
    CMUTIL_StringArray *args = CMUTIL_StringSplitInternal(
                item->memst, extra, ",");
    for (i = 0; i < CMCall(args, GetSize); i++) {
        CMUTIL_String *str = CMCall(args, GetAt, i);
        CMUTIL_StringArray *arr = CMUTIL_StringSplitInternal(
                    item->memst, CMCall(str, GetCString), "=");
        if (CMCall(arr, GetSize) == 2) {
            CMUTIL_String *key = CMCall(arr, GetAt, 0);
            CMUTIL_String *val = CMCall(arr, GetAt, 1);
            const char *ks = CMCall(key, GetCString);
            const char *vs = CMCall(val, GetCString);
            if (strcmp(ks, "length") == 0) {
                int slen = atoi(vs);
                if (slen > 0) {
                    uint32_t len = (uint32_t)slen;
                    CMUTIL_Iterator *iter =
                        CMCall(g_cmutil_log_levels, Iterator);
                    while (CMCall(iter, HasNext)) {
                        CMUTIL_String *v =
                            (CMUTIL_String*)CMCall(iter, Next);
                        const char *key = CMCall(v, GetCString);
                        if (CMCall(map, Get, key) == NULL) {
                            CMUTIL_String *d = CMCall(v, Substring, 0, len);
                            CMCall(map, Put, key, d);
                        }
                    }
                    CMCall(iter, Destroy);
                }
            }
            else if (strcmp(ks, "lowerCase") == 0) {
                CMUTIL_Iterator *iter =
                        CMCall(g_cmutil_log_levels, Iterator);
                while (CMCall(iter, HasNext)) {
                    CMUTIL_String *v =(CMUTIL_String*)CMCall(iter, Next);
                    const char *key = CMCall(v, GetCString);
                    if (CMCall(map, Get, key) == NULL) {
                        CMUTIL_String *d = CMCall(v, ToLower);
                        CMCall(map, Put, key, d);
                    }
                }
                CMCall(iter, Destroy);
            }
            else {
                CMCall(arr, RemoveAt, 1);
                CMUTIL_String *prev =
                        (CMUTIL_String*)CMCall(map, Put, ks, val);
                if (prev)
                    CMCall(prev, Destroy);
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

    // check occurrence of '%'
    while ((q = strchr(p, '%')) != NULL) {
        char buf[128] = { 0, };
        char fmt[20];
        uint32_t length = 0;
        CMBool padleft = CMFalse;
        CMBool hasext = CMFalse;
        CMUTIL_LogFormatType type;
        CMUTIL_LogAppenderFormatItem *item = NULL;
        void *tvoid = NULL;

        // add former string to pattern list
        if (q > p)
            CMCall(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, p, (uint32_t)(q - p)));
        q++;
        // check escape charactor
        if (*q == '%') {
            CMCall(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, "%", 1));
            p = q + 1;
            continue;
        }
        // check the padding length
        if (strchr("+-0123456789", *q)) {
            char *r = buf;
            if (strchr("+-", *q)) {
                if (*q == '-')
                    padleft = CMTrue;
                q++;
            }
            while (strchr("0123456789", *q))
                *r++ = *q++;
            *r = 0x0;
            length = (uint32_t)atoi(buf);
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
                char *milli = NULL;

                if (strchr(buf, '%') == NULL) {
                    const char *fmt = (char*)CMCall(
                            g_cmutil_log_datefmt_map, Get, buf);
                    if (fmt == NULL) {
                        printf("invalid date pattern: %s\n", buf);
                        return CMFalse;
                    }
                    strcpy(buf, fmt);
                }
                if (strstr(buf, "%q")) milli = "%q";
                else if (strstr(buf, "%Q")) milli = "%Q";
                if (milli) {
                    CMUTIL_StringArray *arr =
                            CMUTIL_StringSplitInternal(memst, buf, milli);
                    uint32_t i, size = (uint32_t)CMCall(arr, GetSize);
                    for (i = 0; i < size; i++) {
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
                        item->precision[item->precisioncnt] = (uint32_t)atoi(temp);
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
                sprintf(fmt, "%%%s%dld", padleft ? "" : "-", length);
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
            char buf[1024];
            if (gethostname(buf, 1024) == 0) {
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_String);
                item->data = CMUTIL_StringCreateInternal(memst, 50, NULL);
                CMCall(item->data, AddString, buf);
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
            item->padleft = padleft;
            item->hasext = hasext;
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

CMUTIL_STATIC CMUTIL_Mem *CMUTIL_LogAppenderBaseClean(
        CMUTIL_LogAppenderBase *iap)
{
    CMUTIL_Mem *res = NULL;
    if (iap) {
        res = iap->memst;
        if (iap->writer) {
            iap->isasync = CMFalse;
            CMCall(iap->writer, Join);
        }
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

CMUTIL_STATIC void CMUTIL_LogConsoleAppenderWriter(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    CMUTIL_UNUSED(appender);
    CMUTIL_UNUSED(logtm);
    printf("%s", CMCall(logmsg, GetCString));
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenerFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_List *list = iap->flush_buffer;
    while (CMCall(list, GetSize) > 0) {
        CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                CMCall(list, RemoveFront);
        printf("%s", CMCall(log->log, GetCString));
        CMUTIL_LogAppderAsyncItemDestroy(log);
    }
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenerDestroy(
        CMUTIL_LogAppender *appender)
{
    if (appender) {
        CMUTIL_Mem *memst =
                CMUTIL_LogAppenderBaseClean((CMUTIL_LogAppenderBase*)appender);
        memst->Free(appender);
    }
}

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name, const char *pattern)
{
    CMUTIL_LogAppenderBase *res =
            memst->Alloc(sizeof(CMUTIL_LogAppenderBase));
    memset(res, 0x0, sizeof(CMUTIL_LogAppenderBase));
    res->memst = memst;
    if (!CMUTIL_LogAppenderBaseInit(
                res, pattern, name,
                CMUTIL_LogConsoleAppenderWriter,
                CMUTIL_LogConsoleAppenerFlush,
                CMUTIL_LogConsoleAppenerDestroy)) {
        CMCall((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }
    return (CMUTIL_LogAppender*)res;
}

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreate(
        const char *name, const char *pattern)
{
    return CMUTIL_LogConsoleAppenderCreateInternal(
                CMUTIL_GetMem(), name, pattern);
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
RETRY:
    fp = fopen(iap->fpath, "ab");
    if (fp) {
        fprintf(fp, "%s", CMCall(logmsg, GetCString));
        fclose(fp);
    } else if (!count) {
        CMUTIL_LogPathCreate(iap->fpath);
        count++;
        goto RETRY;
    } else {
        printf("cannot open log file '%s'\n", iap->fpath);
    }
}

CMUTIL_STATIC void CMUTIL_LogFileAppenderFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogFileAppender *iap = (CMUTIL_LogFileAppender*)appender;
    FILE *fp = NULL;
    fp = fopen(iap->fpath, "ab");
    if (fp) {
        CMUTIL_List *list = iap->base.flush_buffer;
        while (CMCall(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                    CMCall(list, RemoveFront);
            fprintf(fp, "%s", CMCall(log->log, GetCString));
            CMUTIL_LogAppderAsyncItemDestroy(log);
        }
        fclose(fp);
    }
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
    CMUTIL_LogTerm  logterm;
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
    char tfname[1024];
    int i = 0;
    strftime(tfname, 1024, appender->rollpath, &(appender->lasttm));
    CMUTIL_LogPathCreate(tfname);

    f = CMUTIL_FileCreateInternal(appender->base.memst, tfname);
    while (CMCall(f, IsExists)) {
        char buf[1024];
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
    if (CMUTIL_LogRollingFileAppenderIsChanged(iap, logtm))
        CMUTIL_LogRollingFileAppenderRolling(iap, logtm);
    fp = fopen(iap->fpath, "ab");
    if (fp) {
        fprintf(fp, "%s", CMCall(logmsg, GetCString));
        fclose(fp);
    } else {
        printf("cannot open log file '%s'.\n", iap->fpath);
    }
}

CMUTIL_STATIC void CMUTIL_LogRollingFileAppenderFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogRollingFileAppender *iap =
            (CMUTIL_LogRollingFileAppender*)appender;
    CMUTIL_List *list = iap->base.flush_buffer;
    if (CMCall(list, GetSize) > 0) {
        FILE *fp = fopen(iap->fpath, "ab");
        while (CMCall(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *item =
                    (CMUTIL_LogAppderAsyncItem*)CMCall(list, RemoveFront);
            if (CMUTIL_LogRollingFileAppenderIsChanged(iap, &item->logtm)) {
                fclose(fp);
                CMUTIL_LogRollingFileAppenderRolling(iap, &item->logtm);
                fp = fopen(iap->fpath, "ab");
            }
            fprintf(fp, "%s", CMCall(item->log, GetCString));
            CMUTIL_LogAppderAsyncItemDestroy(item);
        }
        fclose(fp);
    }
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
        const char *fpath, CMUTIL_LogTerm logterm,
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
    case CMUTIL_LogTerm_Year:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_year) - base; break;
    case CMUTIL_LogTerm_Month:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_mon) - base; break;
    case CMUTIL_LogTerm_Date:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_mday) - base; break;
    case CMUTIL_LogTerm_Hour:
        res->tmst = (CMPtrDiff)&(res->lasttm.tm_hour) - base; break;
    case CMUTIL_LogTerm_Minute:
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
    CMUTIL_LogTerm logterm,
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
    CMUTIL_Mutex        *climtx;
} CMUTIL_LogSocketAppender;

CMUTIL_STATIC void CMUTIL_LogSocketAppenderWrite(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    uint32_t i;
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    CMCall(iap->climtx, Lock);
    for (i = 0; i < CMCall(iap->clients, GetSize); i++) {
        CMUTIL_Socket *cli = (CMUTIL_Socket*)
                CMCall(iap->clients, GetAt, i);
        if (CMCall(cli, Write, logmsg, 1000) != CMUTIL_SocketOk) {
            CMCall(iap->clients, RemoveAt, i);
            i--;
            CMCall(cli, Close);
        }
    }
    CMCall(iap->climtx, Unlock);
    CMUTIL_UNUSED(logtm);
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderFlush(CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
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
        CMCall(iap->climtx, Destroy);
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
        CMUTIL_Socket *cli = CMCall(iap->listener, Accept, 1000);
        if (cli) {
            CMCall(iap->climtx, Lock);
            CMCall(iap->clients, Add, cli);
            CMCall(iap->climtx, Unlock);
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
                memst, 10, NULL, CMUTIL_LogSocketAppenderCliDestroyer, CMFalse);
    res->climtx = CMUTIL_MutexCreateInternal(memst);
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
    CMUTIL_LogSystem	base;
    CMUTIL_Map			*appenders;
    CMUTIL_Array		*cloggers;
    CMUTIL_Array		*loggers;
    CMUTIL_Array		*levelloggers;
    CMUTIL_Mem		*memst;
} CMUTIL_LogSystem_Internal;

typedef struct CMUTIL_ConfLogger_Internal CMUTIL_ConfLogger_Internal;
struct CMUTIL_ConfLogger_Internal
{
    CMUTIL_ConfLogger			base;
    char						*name;
    CMUTIL_LogLevel				level;
    CMBool					additivity;
    CMUTIL_Array				*lvlapndrs;
    const CMUTIL_LogSystem_Internal	*lsys;
    void (*Destroy)(CMUTIL_ConfLogger_Internal *icl);
    CMBool (*Log)(CMUTIL_ConfLogger_Internal *cli,
                       CMUTIL_Logger_Internal *li,
                       CMUTIL_LogLevel level,
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
    CMCall(ilsys->appenders, Put, name, appender);
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
        CMUTIL_LogLevel level,
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
        CMUTIL_LogLevel level)
{
    const CMUTIL_ConfLogger_Internal *cli =
            (const CMUTIL_ConfLogger_Internal*)clogger;
    for ( ;level <= CMUTIL_LogLevel_Fatal; level++) {
        CMUTIL_Array *lapndrs = (CMUTIL_Array*)CMCall(
                    cli->lvlapndrs, GetAt, level);
        CMCall(lapndrs, Add, appender);
    }
}

CMUTIL_STATIC CMUTIL_ConfLogger *CMUTIL_LogSystemCreateLogger(
        const CMUTIL_LogSystem *lsys, const char *name, CMUTIL_LogLevel level,
        CMBool additivity)
{
    CMUTIL_Array *lvllog;
    uint32_t i, lvllen = CMUTIL_LogLevel_Fatal + 1;
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
                   NULL, CMTrue));
    }
    CMCall(ilsys->cloggers, Add, res);
    for (i=level; i<lvllen; i++) {
        lvllog = CMCall(ilsys->levelloggers, GetAt, i);
        CMCall(lvllog, Add, res);
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
    size_t sa = strlen(ca->name), sb = strlen(cb->name);
    // longer name is first.
    if (sa < sb) return 1;
    else if (sa > sb) return -1;
    return 0;
}

CMUTIL_STATIC void CMUTIL_LoggerLogEx(
        CMUTIL_Logger *logger, CMUTIL_LogLevel level,
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
    CMUTIL_Logger_Internal *res;
    CMUTIL_Logger_Internal q;
    q.fullname = CMUTIL_StringCreateInternal(ilsys->memst, 20, name);
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
        res->minlevel = CMUTIL_LogLevel_Fatal;
        for (i=0; i<CMCall(ilsys->cloggers, GetSize); i++) {
            CMUTIL_ConfLogger_Internal *cl = (CMUTIL_ConfLogger_Internal*)
                    CMCall(ilsys->cloggers, GetAt, i);
            // adding root logger and parent loggers
            if (strlen(cl->name) == 0 || strstr(name, cl->name) == 0) {
                CMCall(res->logrefs, Add, cl);
                if (res->minlevel > cl->level)
                    res->minlevel = cl->level;
            }
        }
        // logex and destroyer
        res->base.LogEx = CMUTIL_LoggerLogEx;
        res->Destroy = CMUTIL_LoggerDestroy;
        CMCall(ilsys->loggers, Add, res);
    } else {
        CMCall(q.fullname, Destroy);
    }
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
        ilsys->memst->Free(ilsys);
    }
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
    res->memst = memst;
    res->appenders = CMUTIL_MapCreateInternal(
                memst, 256, CMFalse, CMUTIL_LogAppenderDestroyer);
    res->cloggers = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_ConfLoggerComparator,
                CMUTIL_ConfLoggerDestroyer, CMTrue);
    res->levelloggers = CMUTIL_ArrayCreateInternal(
                memst, CMUTIL_LogLevel_Fatal+1, NULL,
                CMUTIL_DoubleArrayDestroyer, CMFalse);
    for (i=0; i<(CMUTIL_LogLevel_Fatal+1); i++)
        CMCall(res->levelloggers, Add, CMUTIL_ArrayCreateInternal(
                        memst, 3, CMUTIL_ConfLoggerComparator, NULL, CMTrue));
    res->loggers = CMUTIL_ArrayCreateInternal(
                memst, 50, CMUTIL_LoggerComparator,
                CMUTIL_LoggerDestroyer, CMTrue);

    return (CMUTIL_LogSystem*)res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemCreate()
{
    CMCall(g_cmutil_logsystem_mutex, Lock);
    if (__cmutil_logsystem) {
        CMCall(g_cmutil_logsystem_mutex, Destroy);
        __cmutil_logsystem = NULL;
    }
    return CMUTIL_LogSystemCreateInternal(CMUTIL_GetMem());
    CMCall(g_cmutil_logsystem_mutex, Unlock);
}

CMUTIL_STATIC void CMUTIL_LogConfigClean(CMUTIL_Json *json)
{
    uint32_t i;
    switch (CMCall(json, GetType)) {
    case CMUTIL_JsonTypeObject: {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMUTIL_StringArray *keys = CMCall(jobj, GetKeys);
        for (i=0; i<CMCall(keys, GetSize); i++) {
            CMUTIL_String *str = CMCall(keys, GetAt, i);
            const char *key = CMCall(str, GetCString);
            CMUTIL_Json *item = CMCall(jobj, Remove, key);
            // change internal buffer to lowercase
            CMCall(str, SelfToLower);
            // change item recursively.
            CMUTIL_LogConfigClean(item);
            // 'key' is changed to lowercase already.
            // so just put with it.
            CMCall(jobj, Put, key, item);
        }
        CMCall(keys, Destroy);
        break;
    }
    case CMUTIL_JsonTypeArray: {
        CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)json;
        for (i=0; i<CMCall(jarr, GetSize); i++)
            // change item recursively.
            CMUTIL_LogConfigClean(CMCall(jarr, Get, i));
        break;
    }
    default: return;
    }
}

#define CMUITL_LOG_PATTERN_DEFAULT	"%d %P-%t [%F:%L] [%-5p] %c - %m%ex%n"

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
    CMUTIL_LogAppender *conapndr = CMUTIL_LogConsoleAppenderCreateInternal(
                memst, "__CONSOL", CMUITL_LOG_PATTERN_DEFAULT);
    CMUTIL_ConfLogger *rootlogger;
    CMUTIL_LogSystem *res = CMUTIL_LogSystemCreateInternal(memst);
    rootlogger = CMCall(res, CreateLogger, NULL, CMUTIL_LogLevel_Warn,
                          CMTrue);
    CMCall(rootlogger, AddAppender, conapndr, CMUTIL_LogLevel_Warn);
    __cmutil_logsystem = (CMUTIL_LogSystem*)res;
    CMCall(g_cmutil_logsystem_mutex, Unlock);

    CMLogWarn("logging system not initialized or invalid configuration. "
              "using default configuraiton.");
    return res;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessOneAppender(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL, *spattern = NULL;
    CMUTIL_LogAppender *appender = NULL;
    CMBool async = CMFalse;
    int64_t asyncBufsz = 256;

    if (CMCall(json, GetType) != CMUTIL_JsonTypeObject) {
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
    if (CMCall(jobj, Get, "asyncbuffersize")) {
        asyncBufsz = CMCall(jobj, GetLong, "asyncbuffersize");
        if (asyncBufsz < 10) {
            printf("'asyncBufferSize' too small, must be greater than 10\n");
            return CMFalse;
        }
    }

    if (strcmp("console", stype) == 0) {
        appender = CMUTIL_LogConsoleAppenderCreateInternal(
                    lsys->memst, sname, spattern);
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
        CMUTIL_LogTerm rollterm = CMUTIL_LogTerm_Date;
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
                rollterm = CMUTIL_LogTerm_Year;
            } else if (strcasecmp("month", sterm) == 0) {
                rollterm = CMUTIL_LogTerm_Month;
            } else if (strcasecmp("hour", sterm) == 0) {
                rollterm = CMUTIL_LogTerm_Hour;
            } else if (strcasecmp("minute", sterm) == 0) {
                rollterm = CMUTIL_LogTerm_Minute;
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
        const char *apndr, CMUTIL_LogLevel level)
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
        const char *slevel, CMUTIL_LogLevel *level)
{
    if (strcasecmp(slevel, "trace") == 0)
        *level = CMUTIL_LogLevel_Trace;
    else if (strcasecmp(slevel, "debug") == 0)
        *level = CMUTIL_LogLevel_Debug;
    else if (strcasecmp(slevel, "info") == 0)
        *level = CMUTIL_LogLevel_Info;
    else if (strcasecmp(slevel, "warn") == 0)
        *level = CMUTIL_LogLevel_Warn;
    else if (strcasecmp(slevel, "error") == 0)
        *level = CMUTIL_LogLevel_Error;
    else if (strcasecmp(slevel, "fatal") == 0)
        *level = CMUTIL_LogLevel_Fatal;
    else
        return CMFalse;
    return CMTrue;
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessAppenderRef(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_ConfLogger *clogger,
        CMUTIL_Json *json, CMUTIL_LogLevel level)
{
    if (CMCall(json, GetType) == CMUTIL_JsonTypeValue) {
        CMUTIL_JsonValue *jval = (CMUTIL_JsonValue*)json;
        const char *sval = CMCall(jval, GetCString);
        return CMUTIL_LogSystemAddAppenderToLogger(
                    lsys, clogger, sval, level);
    } else {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMUTIL_LogLevel alevel = level;
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
}

CMUTIL_STATIC CMBool CMUTIL_LogSystemProcessOneLogger(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMBool res = CMTrue;
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL;
    CMUTIL_ConfLogger *clogger = NULL;
    CMBool additivity = CMTrue;
    CMUTIL_LogLevel level = CMUTIL_LogLevel_Error;
    CMUTIL_Json *apref = NULL;

    if (CMCall(json, GetType) != CMUTIL_JsonTypeObject) {
        printf("invalid configuration structure.\n");
        return CMFalse;
    }

    if (CMCall(jobj, Get, "name")) {
        CMUTIL_String *name = CMCall(jobj, GetString, "name");
        sname = CMCall(name, GetCString);
        if (*sname == 0x0 && strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            return CMFalse;
        }
    } else {
        if (strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            return CMFalse;
        }
    }
    if (strcmp(stype, "root") != 0) sname = "";
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
        case CMUTIL_JsonTypeArray: {
            CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)apref;
            uint32_t i;
            for (i=0; i<CMCall(jarr, GetSize); i++) {
                CMUTIL_Json *item = CMCall(jarr, Get, i);
                CMUTIL_JsonType type = CMCall(item, GetType);
                if (type == CMUTIL_JsonTypeArray) {
                    printf("invalid appender reference in logger.\n");
                    return CMFalse;
                } else {
                    res = CMUTIL_LogSystemProcessAppenderRef(
                                lsys, clogger, item, level);
                }
                if (!res) {
                    printf("invalid appender reference in logger.\n");
                    return res;
                }
            }
            break;
        }
        case CMUTIL_JsonTypeObject:
            return CMUTIL_LogSystemProcessAppenderRef(
                        lsys, clogger, apref, level);
            break;
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
    if (CMCall(json, GetType) != CMUTIL_JsonTypeArray) {
        printf("invalid configuration structure.\n");
        goto ENDPOINT;
    }
    jarr = (CMUTIL_JsonArray*)json;
    for (i=0; i<CMCall(jarr, GetSize); i++) {
        CMUTIL_Json *item = CMCall(jarr, Get, i);
        CMUTIL_JsonObject *oitem = NULL;
        CMUTIL_String *type = NULL;
        const char *stype = NULL;
        if (CMCall(item, GetType) != CMUTIL_JsonTypeObject) {
            printf("invalid configuration structure.\n");
            goto ENDPOINT;
        }
        oitem = (CMUTIL_JsonObject*)item;
        type = CMCall(oitem, GetString, "type");
        CMCall(type, SelfToLower);
        stype = CMCall(type, GetCString);

        if (!oneprocf(lsys, stype, item)) {
            printf("processing one item failed.\n");
            goto ENDPOINT;
        }
    }
    res = CMTrue;
ENDPOINT:
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureInternal(
        CMUTIL_Mem *memst, CMUTIL_Json *json)
{
    CMUTIL_LogSystem *lsys;
    CMUTIL_JsonObject *config = (CMUTIL_JsonObject*)json;
    CMUTIL_Json *appenders, *loggers;
    CMBool succeeded = CMFalse;

    // all keys to lowercase for case insensitive.
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
    CMUTIL_File *f = CMUTIL_FileCreateInternal(memst, jsonfile);
    CMUTIL_String *jsonstr = CMCall(f, GetContents);
    CMUTIL_Json *json = CMUTIL_JsonParseInternal(memst, jsonstr, CMTrue);
    CMUTIL_LogSystem *res = CMUTIL_LogSystemConfigureInternal(memst, json);
    CMCall(json, Destroy);
    CMCall(jsonstr, Destroy);
    CMCall(f, Destroy);
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(const char *jsonfile)
{
    return CMUTIL_LogSystemConfigureFomJsonInternal(
                CMUTIL_GetMem(), jsonfile);
}

static CMBool g_cmutil_logsystem_initializing = CMFalse;

CMUTIL_LogSystem *CMUTIL_LogSystemGetInternal(CMUTIL_Mem *memst)
{
    if (__cmutil_logsystem) return __cmutil_logsystem;

    CMCall(g_cmutil_logsystem_mutex, Lock);
    if (g_cmutil_logsystem_initializing) return NULL;
    g_cmutil_logsystem_initializing = CMTrue;
    CMCall(g_cmutil_logsystem_mutex, Unlock);

    CMCall(g_cmutil_logsystem_mutex, Lock);
    if (__cmutil_logsystem == NULL) {
        // try to read cmutil_log.json
        char *conf = getenv("CMUTIL_LOG_CONFIG");
        if (!conf) conf = "cmutil_log.json";
        CMUTIL_LogSystemConfigureFomJsonInternal(memst, conf);
    }
    CMCall(g_cmutil_logsystem_mutex, Unlock);
    g_cmutil_logsystem_initializing =
            __cmutil_logsystem == NULL? CMTrue:CMFalse;
    return __cmutil_logsystem;
}

CMUTIL_LogSystem *CMUTIL_LogSystemGet()
{
    return CMUTIL_LogSystemGetInternal(CMUTIL_GetMem());
}
