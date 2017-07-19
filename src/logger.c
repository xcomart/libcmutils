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
    CMUTIL_String				*data;
    CMUTIL_String				*data2;
    int							length;
    CMUTIL_Bool					padleft;
    int							precision[10];
    int							precisioncnt;
    CMUTIL_Bool					hasext;
    void						*anything;
    CMUTIL_Mem				*memst;
};

typedef struct CMUTIL_LogAppenderBase CMUTIL_LogAppenderBase;
struct CMUTIL_LogAppenderBase {
    CMUTIL_LogAppender	base;
    void(*Write)(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg,
        struct tm *logtm);
    char				*name;
    int					buffersz;
    CMUTIL_List			*pattern;
    CMUTIL_List			*buffer;
    CMUTIL_List			*flush_buffer;
    CMUTIL_Thread		*writer;
    CMUTIL_Bool			isasync;
    CMUTIL_Mutex		*mutex;
    CMUTIL_Mem		*memst;
};

typedef struct CMUTIL_Logger_Internal CMUTIL_Logger_Internal;
struct CMUTIL_Logger_Internal {
    CMUTIL_Logger				base;
    CMUTIL_String				*fullname;
    CMUTIL_StringArray			*namepath;
    CMUTIL_Array				*logrefs;
    CMUTIL_Mem				*memst;
    CMUTIL_LogLevel				minlevel;
    void (*Destroy)(CMUTIL_Logger_Internal *logger);
};

typedef struct CMUTIL_LogPatternAppendFuncParam {
    CMUTIL_LogAppenderFormatItem	*item;
    CMUTIL_String					*dest;
    CMUTIL_Logger					*logger;
    CMUTIL_LogLevel					level;
    CMUTIL_StringArray				*stack;
    struct tm						*logtm;
    long							millis;
    const char						*fname;
    int								linenum;
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
        CMUTIL_CALL(ii->data, Destroy);
    if (ii->data2)
        CMUTIL_CALL(ii->data2, Destroy);
    if (ii->type == LogFormatItem_LogLevel && ii->anything)
        CMUTIL_CALL((CMUTIL_Map*)ii->anything, Destroy);
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
    const char *data, int length)
{
    if (item->length > length) {
        if (item->padleft) {
            CMUTIL_CALL(dest, AddNString,
                        g_cmutil_spaces, item->length - length);
            CMUTIL_CALL(dest, AddNString,
                        data, length);
        }
        else {
            CMUTIL_CALL(dest, AddNString,
                        data, length);
            CMUTIL_CALL(dest, AddNString,
                        g_cmutil_spaces, item->length - length);
        }
    }
    else {
        CMUTIL_CALL(dest, AddNString, data, length);
    }
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendDateTime(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    char buf[50];
    int sz;
    const char *fmt = CMUTIL_CALL(params->item->data, GetCString);
    sz = (int)strftime(buf, 50, fmt, params->logtm);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, sz);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendMillisec(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_CALL(params->dest, AddPrint, "%03d", params->millis);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendString(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMUTIL_CALL(params->item->data, GetCString),
        CMUTIL_CALL(params->item->data, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLoggerName(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_Logger_Internal *ilog = (CMUTIL_Logger_Internal*)params->logger;
    CMUTIL_String *buf = CMUTIL_StringCreateInternal(ilog->memst, 10, NULL);
    CMUTIL_String *curr = NULL;
    int i, tsize = CMUTIL_CALL(ilog->namepath, GetSize);
    switch (params->item->precisioncnt) {
    case 0:
        CMUTIL_CALL(buf, AddAnother, ilog->fullname);
        break;
    case 1: {
        int lastcnt = params->item->precision[0];
        for (i = tsize - lastcnt; i < (tsize - 1); i++) {
            curr = CMUTIL_CALL(ilog->namepath, GetAt, i);
            CMUTIL_CALL(buf, AddAnother, curr);
            CMUTIL_CALL(buf, AddChar, '.');
        }
        curr = CMUTIL_CALL(ilog->namepath, GetAt, tsize - 1);
        CMUTIL_CALL(buf, AddAnother, curr);
        break;
    }
    case 2: {
        int i, size = params->item->precision[0];
        for (i = 0; i < (tsize - 1); i++) {
            if (size) {
                int minsz = size;
                const char *currstr;
                curr = CMUTIL_CALL(ilog->namepath, GetAt, i);
                currstr = CMUTIL_CALL(curr, GetCString);
                if (CMUTIL_CALL(curr, GetSize) < size)
                    minsz = CMUTIL_CALL(curr, GetSize);
                CMUTIL_CALL(buf, AddNString, currstr, minsz);
            }
            CMUTIL_CALL(buf, AddChar, '.');
        }
        curr = CMUTIL_CALL(ilog->namepath, GetAt, tsize - 1);
        CMUTIL_CALL(buf, AddAnother, curr);
        break;
    }
    default:
        CMUTIL_CALL(buf, AddAnother, ilog->fullname);
        break;
    }

    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMUTIL_CALL(buf, GetCString),
        CMUTIL_CALL(buf, GetSize));

    CMUTIL_CALL(buf, Destroy);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendThreadId(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    char buf[256];
    CMUTIL_Thread *t = CMUTIL_ThreadSelf();
    unsigned int tid = CMUTIL_CALL(t, GetId);
    const char *name = CMUTIL_CALL(t, GetName);
    int size = sprintf(buf, "%s-%u", name, tid);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendFileName(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    int tsize = (int)strlen(params->fname), size = 0;
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
    int size = sprintf(buf, "%d", params->linenum);
    CMUTIL_LogPatternAppendPadding(params->item, params->dest, buf, size);
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendLogLevel(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_String *val = CMUTIL_CALL(g_cmutil_log_levels, GetAt, params->level);
    if (params->item->hasext) {
        CMUTIL_Map *lvlmap = (CMUTIL_Map*)params->item->anything;
        CMUTIL_String *key = val;
        const char *skey = CMUTIL_CALL(key, GetCString);
        val = CMUTIL_CALL(lvlmap, Get, skey);
        if (val == NULL) val = key;
    }
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMUTIL_CALL(val, GetCString),
        CMUTIL_CALL(val, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendMessage(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    CMUTIL_LogPatternAppendPadding(
        params->item, params->dest,
        CMUTIL_CALL(params->logmsg, GetCString),
        CMUTIL_CALL(params->logmsg, GetSize));
}

CMUTIL_STATIC void CMUTIL_LogPatternAppendStack(
    CMUTIL_LogPatternAppendFuncParam *params)
{
    if (params->stack != NULL && CMUTIL_CALL(params->stack, GetSize) > 0) {
        int i, maxlen = params->item->hasext ?
            params->item->precisioncnt : CMUTIL_CALL(params->stack, GetSize);
        for (i = 0; i < maxlen; i++) {
            CMUTIL_String *item = CMUTIL_CALL(params->stack, GetAt, i);
            CMUTIL_CALL(params->dest, AddString, S_CRLF);
            CMUTIL_CALL(params->dest, AddAnother, item);
        }
    }
}

void CMUTIL_LogInit()
{
    int i;
    g_cmutil_log_item_map = CMUTIL_MapCreateInternal(
                CMUTIL_GetMem(), 50, CMUTIL_False, NULL);
    for (i = 0; i < LogType_Length; i++)
        g_cmutil_log_type_arr[i] = i;

#define LOGTYPE_ADD(a, b) CMUTIL_CALL(g_cmutil_log_item_map, Put, a, \
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
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "DEFAULT", "%F %T.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "ISO8601", "%FT%T.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put,
                "ISO8601_BASIC", "%Y%m%dT%H%M%S.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "ABSOLUTE", "%T.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "DATE", "%d %b %Y %T.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "COMPACT", "%Y%m%d%H%M%S.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "GENERAL", "%Y%m%d %H%M%S.%Q");
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Put, "UNIX", "%s%Q");

    g_cmutil_log_levels = CMUTIL_StringArrayCreate();
    CMUTIL_CALL(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "TRACE"));
    CMUTIL_CALL(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "DEBUG"));
    CMUTIL_CALL(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "INFO" ));
    CMUTIL_CALL(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "WARN" ));
    CMUTIL_CALL(g_cmutil_log_levels, Add,
                CMUTIL_StringCreateInternal(CMUTIL_GetMem(), 5, "ERROR"));
    CMUTIL_CALL(g_cmutil_log_levels, Add,
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
    CMUTIL_CALL(g_cmutil_log_item_map, Destroy);
    g_cmutil_log_item_map = NULL;
    CMUTIL_CALL(g_cmutil_log_datefmt_map, Destroy);
    g_cmutil_log_datefmt_map = NULL;
    CMUTIL_CALL(g_cmutil_log_levels, Destroy);
    g_cmutil_log_levels = NULL;
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Destroy);
    g_cmutil_logsystem_mutex = NULL;
    if (__cmutil_logsystem) {
        CMUTIL_CALL(__cmutil_logsystem, Destroy);
        __cmutil_logsystem = NULL;
    }
}

CMUTIL_STATIC const char *CMUTIL_LogAppenderBaseGetName(
    CMUTIL_LogAppender *appender)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    return iap->name;
}

CMUTIL_STATIC void CMUTIL_LogAppenderStringDestroyer(void *data)
{
    CMUTIL_CALL((CMUTIL_String*)data, Destroy);
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
        CMUTIL_CALL(iap->mutex, Lock);
        iap->flush_buffer = iap->buffer;
        iap->buffer = temp;
        CMUTIL_CALL(iap->mutex, Unlock);
        CMUTIL_CALL((CMUTIL_LogAppender*)iap, Flush);
        USLEEP(100000);	// sleep 100 milliseconds
    }
    CMUTIL_CALL((CMUTIL_LogAppender*)iap, Flush);
    temp = iap->flush_buffer;
    CMUTIL_CALL(iap->mutex, Lock);
    iap->flush_buffer = iap->buffer;
    iap->buffer = temp;
    CMUTIL_CALL(iap->mutex, Unlock);
    CMUTIL_CALL((CMUTIL_LogAppender*)iap, Flush);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_LogAppenderBaseSetAsync(
    CMUTIL_LogAppender *appender, int buffersz)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_CALL(iap->mutex, Lock);
    if (!iap->isasync) {
        iap->buffersz = buffersz;
        iap->isasync = CMUTIL_True;
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
            CMUTIL_CALL(iap->writer, Start);
        }
    }
    CMUTIL_CALL(iap->mutex, Unlock);
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
        if (item->log) CMUTIL_CALL(item->log, Destroy);
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
    iter = CMUTIL_CALL(iap->pattern, Iterator);
    while (CMUTIL_CALL(iter, HasNext)) {
        param.item = (CMUTIL_LogAppenderFormatItem*)CMUTIL_CALL(iter, Next);
        g_cmutil_log_pattern_funcs[param.item->type](&param);
    }
    CMUTIL_CALL(iter, Destroy);

    CMUTIL_CALL(iap->mutex, Lock);
    if (iap->isasync) {
        if (CMUTIL_CALL(iap->buffer, GetSize) >= iap->buffersz) {
            CMUTIL_CALL(iap->mutex, Unlock);
            CMUTIL_CALL(param.dest, Destroy);
            // TODO: error logging - async buffer full.
            printf("Warning! '%s' async buffer is full.\n", iap->name);
            return;
        }
        else {
            CMUTIL_CALL(iap->buffer, AddTail,
                CMUTIL_LogAppderAsyncItemCreate(
                            iap->memst, param.dest, &currtm));
        }
    }
    else {
        CMUTIL_CALL(iap, Write, param.dest, &currtm);
        CMUTIL_CALL(param.dest, Destroy);
    }
    CMUTIL_CALL(iap->mutex, Unlock);
}

CMUTIL_STATIC CMUTIL_LogAppenderFormatItem *CMUTIL_LogAppenderItemString(
    CMUTIL_Mem *memst, const char *a, int length)
{
    CMUTIL_LogAppenderFormatItem *res =
        CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
    res->data = CMUTIL_StringCreate();
    CMUTIL_CALL(res->data, AddNString, a, length);
    return res;
}

CMUTIL_STATIC void CMUTIL_LogPatternLogLevelParse(
    CMUTIL_LogAppenderFormatItem *item, const char *extra)
{
    int i;
    CMUTIL_Map *map = CMUTIL_MapCreateInternal(
                item->memst, 10, CMUTIL_False,
                CMUTIL_LogAppenderStringDestroyer);
    CMUTIL_StringArray *args = CMUTIL_StringSplitInternal(
                item->memst, extra, ",");
    for (i = 0; i < CMUTIL_CALL(args, GetSize); i++) {
        CMUTIL_String *str = CMUTIL_CALL(args, GetAt, i);
        CMUTIL_StringArray *arr = CMUTIL_StringSplitInternal(
                    item->memst, CMUTIL_CALL(str, GetCString), "=");
        if (CMUTIL_CALL(arr, GetSize) == 2) {
            CMUTIL_String *key = CMUTIL_CALL(arr, GetAt, 0);
            CMUTIL_String *val = CMUTIL_CALL(arr, GetAt, 1);
            const char *ks = CMUTIL_CALL(key, GetCString);
            const char *vs = CMUTIL_CALL(val, GetCString);
            if (strcmp(ks, "length") == 0) {
                int len = atoi(vs);
                CMUTIL_Iterator *iter =
                    CMUTIL_CALL(g_cmutil_log_levels, Iterator);
                while (CMUTIL_CALL(iter, HasNext)) {
                    CMUTIL_String *v =
                        (CMUTIL_String*)CMUTIL_CALL(iter, Next);
                    const char *key = CMUTIL_CALL(v, GetCString);
                    if (CMUTIL_CALL(map, Get, key) == NULL) {
                        CMUTIL_String *d = CMUTIL_CALL(v, Substring, 0, len);
                        CMUTIL_CALL(map, Put, key, d);
                    }
                }
                CMUTIL_CALL(iter, Destroy);
            }
            else if (strcmp(ks, "lowerCase") == 0) {
                CMUTIL_Iterator *iter =
                        CMUTIL_CALL(g_cmutil_log_levels, Iterator);
                while (CMUTIL_CALL(iter, HasNext)) {
                    CMUTIL_String *v =(CMUTIL_String*)CMUTIL_CALL(iter, Next);
                    const char *key = CMUTIL_CALL(v, GetCString);
                    if (CMUTIL_CALL(map, Get, key) == NULL) {
                        CMUTIL_String *d = CMUTIL_CALL(v, ToLower);
                        CMUTIL_CALL(map, Put, key, d);
                    }
                }
                CMUTIL_CALL(iter, Destroy);
            }
            else {
                CMUTIL_CALL(arr, RemoveAt, 1);
                CMUTIL_String *prev =
                        (CMUTIL_String*)CMUTIL_CALL(map, Put, ks, val);
                if (prev)
                    CMUTIL_CALL(prev, Destroy);
            }
        }
        else {
            // TODO: error report
        }
        CMUTIL_CALL(arr, Destroy);
    }

    CMUTIL_CALL(args, Destroy);
    item->anything = map;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogPatternParse(
    CMUTIL_Mem *memst, CMUTIL_List *list, const char *pattern)
{
    const char *p, *q, *r;
    p = pattern;

    // check occurrence of '%'
    while ((q = strchr(p, '%')) != NULL) {
        char buf[128] = { 0, };
        char fmt[20];
        int length = 0;
        CMUTIL_Bool padleft = CMUTIL_False;
        CMUTIL_Bool hasext = CMUTIL_False;
        CMUTIL_LogFormatType type;
        CMUTIL_LogAppenderFormatItem *item = NULL;
        void *tvoid = NULL;

        // add former string to pattern list
        if (q > p)
            CMUTIL_CALL(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, p, (int)(q - p)));
        q++;
        // check escape charactor
        if (*q == '%') {
            CMUTIL_CALL(list, AddTail,
                CMUTIL_LogAppenderItemString(memst, "%", 1));
            p = q + 1;
            continue;
        }
        // check the padding length
        if (strchr("+-0123456789", *q)) {
            char *r = buf;
            if (strchr("+-", *q)) {
                if (*q == '-')
                    padleft = CMUTIL_True;
                q++;
            }
            while (strchr("0123456789", *q))
                *r++ = *q++;
            *r = 0x0;
            length = atoi(buf);
        }
        // check the pattern item
        q = CMUTIL_StrNextToken(
                    buf, 20, q, " \t\r\n{}[]:;,.!@#$%^&*()-+_=/\\|");
        tvoid = CMUTIL_CALL(g_cmutil_log_item_map, Get, buf);
        if (!tvoid) {
            printf("unknown pattern %%%s\n", buf);
            return CMUTIL_False;
        }
        type = *(CMUTIL_LogFormatType*)tvoid;
        *buf = 0x0;
        if (*q == '{') {
            r = q + 1;
            q = strchr(r, '}');
            if (q) {
                strncat(buf, r, (size_t)(q - r));
                q++;
                hasext = CMUTIL_True;
            }
            else {
                printf("pattern brace mismatch.\n");
                return CMUTIL_False;
            }
        }
        switch (type) {
        case LogType_DateTime:
            if (hasext) {
                char *milli = NULL;

                if (strchr(buf, '%') == NULL) {
                    const char *fmt = (char*)CMUTIL_CALL(
                            g_cmutil_log_datefmt_map, Get, buf);
                    if (fmt == NULL) {
                        printf("invalid date pattern: %s\n", buf);
                        return CMUTIL_False;
                    }
                    strcpy(buf, fmt);
                }
                if (strstr(buf, "%q")) milli = "%q";
                else if (strstr(buf, "%Q")) milli = "%Q";
                if (milli) {
                    CMUTIL_StringArray *arr =
                            CMUTIL_StringSplitInternal(memst, buf, milli);
                    int i, size = CMUTIL_CALL(arr, GetSize);
                    for (i = 0; i < size; i++) {
                        CMUTIL_String *s = CMUTIL_CALL(arr, RemoveAt, 0);
                        if (CMUTIL_CALL(s, GetSize) > 0) {
                            item = CMUTIL_LogAppenderItemCreate(
                                        memst, LogFormatItem_DateTime);
                            item->data = s;
                            CMUTIL_CALL(list, AddTail, item);
                        }
                        else {
                            CMUTIL_CALL(s, Destroy);
                        }
                        if (i < (size - 1)){
                            item = CMUTIL_LogAppenderItemCreate(
                                        memst, LogFormatItem_Millisec);
                            item->hasext =
                                *(milli + 1) == 'Q' ?
                                        CMUTIL_True : CMUTIL_False;
                            CMUTIL_CALL(list, AddTail, item);
                        }
                    }
                    CMUTIL_CALL(arr, Destroy);
                }
                else {
                    item = CMUTIL_LogAppenderItemCreate(
                                memst, LogFormatItem_DateTime);
                    item->data = CMUTIL_StringCreateInternal(memst, 20, buf);
                    CMUTIL_CALL(list, AddTail, item);
                }
            }
            else {
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_DateTime);
                item->data = CMUTIL_StringCreateInternal(memst, 7, "%F %T.");
                item->hasext = CMUTIL_True;
                CMUTIL_CALL(list, AddTail, item);
                item = CMUTIL_LogAppenderItemCreate(
                            memst, LogFormatItem_Millisec);
                item->hasext = CMUTIL_True;
                CMUTIL_CALL(list, AddTail, item);
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
                        item->precision[item->precisioncnt] = atoi(temp);
                    }
                    item->precisioncnt++;
                    p = r + 1;
                }
                if (item->precisioncnt == 0) {
                    item->precision[item->precisioncnt] = atoi(buf);
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
            CMUTIL_CALL(item->data, AddPrint, fmt, GETPID());
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
                CMUTIL_CALL(item->data, AddPrint, fmt, getenv(buf));
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
                CMUTIL_CALL(item->data, AddString, buf);
            }
            break;
        }
        case LogType_Stack:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_Stack);
            if (hasext)
                item->precisioncnt = atoi(buf);
            break;
        case LogType_LineSeparator:
            item = CMUTIL_LogAppenderItemCreate(memst, LogFormatItem_String);
            item->data = CMUTIL_StringCreateInternal(memst, 3, NULL);
            CMUTIL_CALL(item->data, AddString, S_CRLF);
            break;
        default:
            printf("unknown pattern type.\n");
            return CMUTIL_False;
        }
        if (item) {
            item->length = length;
            item->padleft = padleft;
            item->hasext = hasext;
            CMUTIL_CALL(list, AddTail, item);
        }
        p = q;
    }
    return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogAppenderBaseInit(
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
        return CMUTIL_False;
    iap->Write = WriteFn;
    iap->name = iap->memst->Strdup(name);
    iap->mutex = CMUTIL_MutexCreateInternal(iap->memst);
    iap->base.GetName = CMUTIL_LogAppenderBaseGetName;
    iap->base.SetAsync = CMUTIL_LogAppenderBaseSetAsync;
    iap->base.Append = CMUTIL_LogAppenderBaseAppend;
    iap->base.Flush = FlushFn;
    iap->base.Destroy = DestroyFn;
    return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Mem *CMUTIL_LogAppenderBaseClean(
        CMUTIL_LogAppenderBase *iap)
{
    CMUTIL_Mem *res = NULL;
    if (iap) {
        res = iap->memst;
        if (iap->writer) {
            iap->isasync = CMUTIL_False;
            CMUTIL_CALL(iap->writer, Join);
        }
        if (iap->buffer)
            CMUTIL_CALL(iap->buffer, Destroy);
        if (iap->flush_buffer)
            CMUTIL_CALL(iap->flush_buffer, Destroy);
        if (iap->pattern)
            CMUTIL_CALL(iap->pattern, Destroy);
        if (iap->name)
            res->Free(iap->name);
        if (iap->mutex)
            CMUTIL_CALL(iap->mutex, Destroy);
    }
    return res;
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenderWriter(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    CMUTIL_UNUSED(appender);
    CMUTIL_UNUSED(logtm);
    printf("%s", CMUTIL_CALL(logmsg, GetCString));
}

CMUTIL_STATIC void CMUTIL_LogConsoleAppenerFlush(
        CMUTIL_LogAppender *appender)
{
    CMUTIL_LogAppenderBase *iap = (CMUTIL_LogAppenderBase*)appender;
    CMUTIL_List *list = iap->flush_buffer;
    while (CMUTIL_CALL(list, GetSize) > 0) {
        CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                CMUTIL_CALL(list, RemoveFront);
        printf("%s", CMUTIL_CALL(log->log, GetCString));
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
        CMUTIL_CALL((CMUTIL_LogAppender*)res, Destroy);
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
        fprintf(fp, "%s", CMUTIL_CALL(logmsg, GetCString));
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
        while (CMUTIL_CALL(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *log = (CMUTIL_LogAppderAsyncItem*)
                    CMUTIL_CALL(list, RemoveFront);
            fprintf(fp, "%s", CMUTIL_CALL(log->log, GetCString));
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
        CMUTIL_CALL((CMUTIL_LogAppender*)res, Destroy);
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
    CMUTIL_LogTerm logterm;
    char *fpath;
    char *rollpath;
    struct tm lasttm;
    CMUTIL_PointDiff tmst;
    CMUTIL_PointDiff tmlen;
} CMUTIL_LogRollingFileAppender;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogRollingFileAppenderIsChanged(
        CMUTIL_LogRollingFileAppender *appender, struct tm *curr)
{
    return memcmp(
                (char*)&(appender->lasttm) + appender->tmst,
                (char*)curr + appender->tmst,
                (size_t)appender->tmlen) == 0? CMUTIL_False:CMUTIL_True;
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
    while (CMUTIL_CALL(f, IsExists)) {
        char buf[1024];
        sprintf(buf, "%s-%d", tfname, i++);
        CMUTIL_CALL(f, Destroy);
        f = CMUTIL_FileCreateInternal(appender->base.memst, buf);
    }
    rename(appender->fpath, CMUTIL_CALL(f, GetFullPath));
    CMUTIL_CALL(f, Destroy);
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
        fprintf(fp, "%s", CMUTIL_CALL(logmsg, GetCString));
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
    if (CMUTIL_CALL(list, GetSize) > 0) {
        FILE *fp = fopen(iap->fpath, "ab");
        while (CMUTIL_CALL(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *item =
                    (CMUTIL_LogAppderAsyncItem*)CMUTIL_CALL(list, RemoveFront);
            if (CMUTIL_LogRollingFileAppenderIsChanged(iap, &item->logtm)) {
                fclose(fp);
                CMUTIL_LogRollingFileAppenderRolling(iap, &item->logtm);
                fp = fopen(iap->fpath, "ab");
            }
            fprintf(fp, "%s", CMUTIL_CALL(item->log, GetCString));
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
    CMUTIL_PointDiff base;
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
        CMUTIL_CALL((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }

    res->fpath = memst->Strdup(fpath);
    res->rollpath = memst->Strdup(rollpath);

    base = (CMUTIL_PointDiff)&(res->lasttm);
    switch (logterm) {
    case CMUTIL_LogTerm_Year:
        res->tmst = (CMUTIL_PointDiff)&(res->lasttm.tm_year) - base; break;
    case CMUTIL_LogTerm_Month:
        res->tmst = (CMUTIL_PointDiff)&(res->lasttm.tm_mon) - base; break;
    case CMUTIL_LogTerm_Date:
        res->tmst = (CMUTIL_PointDiff)&(res->lasttm.tm_mday) - base; break;
    case CMUTIL_LogTerm_Hour:
        res->tmst = (CMUTIL_PointDiff)&(res->lasttm.tm_hour) - base; break;
    case CMUTIL_LogTerm_Minute:
        res->tmst = (CMUTIL_PointDiff)&(res->lasttm.tm_min) - base; break;
    }
    res->tmlen = (CMUTIL_PointDiff)&(res->lasttm.tm_wday) - base - res->tmst;

    file = CMUTIL_FileCreateInternal(memst, fpath);
    ctime = time(NULL);
    localtime_r(&ctime, &curr);
    if (CMUTIL_CALL(file, IsExists)) {
        ptime = CMUTIL_CALL(file, ModifiedTime);
        localtime_r(&ptime, &(res->lasttm));
        if (CMUTIL_LogRollingFileAppenderIsChanged(res, &curr))
            CMUTIL_LogRollingFileAppenderRolling(res, &curr);
    } else {
        localtime_r(&ctime, &(res->lasttm));
    }
    CMUTIL_CALL(file, Destroy);
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
    CMUTIL_Bool isrunning;
    CMUTIL_ServerSocket *listener;
    CMUTIL_Thread *acceptor;
    CMUTIL_Array *clients;
    CMUTIL_Mutex *climtx;
} CMUTIL_LogSocketAppender;

CMUTIL_STATIC void CMUTIL_LogSocketAppenderWrite(
        CMUTIL_LogAppenderBase *appender,
        CMUTIL_String *logmsg, struct tm *logtm)
{
    int i;
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    CMUTIL_CALL(iap->climtx, Lock);
    for (i = 0; i < CMUTIL_CALL(iap->clients, GetSize); i++) {
        CMUTIL_Socket *cli = (CMUTIL_Socket*)
                CMUTIL_CALL(iap->clients, GetAt, i);
        if (CMUTIL_CALL(cli, Write, logmsg, 1000) != CMUTIL_SocketOk) {
            CMUTIL_CALL(iap->clients, RemoveAt, i);
            i--;
            CMUTIL_CALL(cli, Close);
        }
    }
    CMUTIL_CALL(iap->climtx, Unlock);
    CMUTIL_UNUSED(logtm);
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderFlush(CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    CMUTIL_List *list = iap->base.flush_buffer;
    if (CMUTIL_CALL(list, GetSize) > 0) {
        CMUTIL_String *logbuf =
                CMUTIL_StringCreateInternal(iap->base.memst, 1024, NULL);
        while (CMUTIL_CALL(list, GetSize) > 0) {
            CMUTIL_LogAppderAsyncItem *item = (CMUTIL_LogAppderAsyncItem*)
                    CMUTIL_CALL(list, RemoveFront);
            CMUTIL_CALL(logbuf, AddAnother, item->log);
            CMUTIL_LogAppderAsyncItemDestroy(item);
        }
        CMUTIL_LogSocketAppenderWrite(
                    (CMUTIL_LogAppenderBase*)appender, logbuf, NULL);
        CMUTIL_CALL(logbuf, Destroy);
    }
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderDestroy(CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)appender;
    if (iap) {
        iap->isrunning = CMUTIL_False;
        CMUTIL_Mem *memst =
                CMUTIL_LogAppenderBaseClean((CMUTIL_LogAppenderBase*)iap);
        CMUTIL_CALL(iap->listener, Close);
        CMUTIL_CALL(iap->acceptor, Join);
        CMUTIL_CALL(iap->clients, Destroy);
        CMUTIL_CALL(iap->climtx, Destroy);
        memst->Free(iap);
    }
}

CMUTIL_STATIC void CMUTIL_LogSocketAppenderCliDestroyer(void *dat)
{
    CMUTIL_CALL((CMUTIL_Socket*)dat, Close);
}

CMUTIL_STATIC void *CMUTIL_LogSocketAppenderAcceptor(void *data)
{
    CMUTIL_LogSocketAppender *iap = (CMUTIL_LogSocketAppender*)data;
    while (iap->isrunning) {
        CMUTIL_Socket *cli = CMUTIL_CALL(iap->listener, Accept, 1000);
        if (cli) {
            CMUTIL_CALL(iap->climtx, Lock);
            CMUTIL_CALL(iap->clients, Add, cli);
            CMUTIL_CALL(iap->climtx, Unlock);
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
        CMUTIL_CALL((CMUTIL_LogAppender*)res, Destroy);
        return NULL;
    }
    res->listener = CMUTIL_ServerSocketCreateInternal(
                memst, accept_host, listen_port, 256, CMUTIL_True);
    if (!res->listener) goto FAILED;
    res->clients = CMUTIL_ArrayCreateInternal(
                memst, 10, NULL, CMUTIL_LogSocketAppenderCliDestroyer);
    res->climtx = CMUTIL_MutexCreateInternal(memst);
    sprintf(namebuf, "LogAppender(%s)-SocketAcceptor", name);
    res->isrunning = CMUTIL_True;
    res->acceptor = CMUTIL_ThreadCreateInternal(
                memst, CMUTIL_LogSocketAppenderAcceptor, res,
                namebuf);
    CMUTIL_CALL(res->acceptor, Start);

    return (CMUTIL_LogAppender*)res;
FAILED:
    CMUTIL_CALL((CMUTIL_LogAppender*)res, Destroy);
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
    CMUTIL_Bool					additivity;
    CMUTIL_Array				*lvlapndrs;
    CMUTIL_LogSystem_Internal	*lsys;
    void (*Destroy)(CMUTIL_ConfLogger_Internal *icl);
    CMUTIL_Bool (*Log)(CMUTIL_ConfLogger_Internal *cli,
                       CMUTIL_Logger_Internal *li,
                       CMUTIL_LogLevel level,
                       CMUTIL_StringArray *stack,
                       const char *file,
                       int line,
                       CMUTIL_String *logmsg);
};

CMUTIL_STATIC void CMUTIL_LogSystemAddAppender(
        CMUTIL_LogSystem *lsys, CMUTIL_LogAppender *appender)
{
    CMUTIL_LogSystem_Internal *ilsys = (CMUTIL_LogSystem_Internal*)lsys;
    const char *name = CMUTIL_CALL(appender, GetName);
    CMUTIL_CALL(ilsys->appenders, Put, name, appender);
}

CMUTIL_STATIC void CMUTIL_DoubleArrayDestroyer(void *data)
{
    CMUTIL_Array *arr = (CMUTIL_Array*)data;
    CMUTIL_CALL(arr, Destroy);
}

CMUTIL_STATIC void CMUTIL_ConfLoggerDestroy(CMUTIL_ConfLogger_Internal *icl)
{
    if (icl) {
        if (icl->name)
            icl->lsys->memst->Free(icl->name);
        if (icl->lvlapndrs)
            CMUTIL_CALL(icl->lvlapndrs, Destroy);
        icl->lsys->memst->Free(icl);
    }
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ConfLoggerLog(
        CMUTIL_ConfLogger_Internal *cli,
        CMUTIL_Logger_Internal *li,
        CMUTIL_LogLevel level,
        CMUTIL_StringArray *stack,
        const char *file,
        int line,
        CMUTIL_String *logmsg)
{
    CMUTIL_Array *apndrs = CMUTIL_CALL(cli->lvlapndrs, GetAt, level);
    if (CMUTIL_CALL(apndrs, GetSize) > 0) {
        int i;
        for (i=0; i<CMUTIL_CALL(apndrs, GetSize); i++) {
            CMUTIL_LogAppender *ap =
                    (CMUTIL_LogAppender*)CMUTIL_CALL(apndrs, GetAt, i);
            CMUTIL_CALL(ap, Append, &li->base, level, stack,
                        file, line, logmsg);
        }
        return CMUTIL_True;
    } else {
        return CMUTIL_False;
    }
}

CMUTIL_STATIC int CMUTIL_LogAppenderComparator(const void *a, const void *b)
{
    CMUTIL_LogAppender *la = (CMUTIL_LogAppender*)a;
    CMUTIL_LogAppender *lb = (CMUTIL_LogAppender*)b;
    return strcmp(CMUTIL_CALL(la, GetName), CMUTIL_CALL(lb, GetName));
}

CMUTIL_STATIC void CMUTIL_ConfLoggerAddAppender(
        CMUTIL_ConfLogger *clogger,
        CMUTIL_LogAppender *appender,
        CMUTIL_LogLevel level)
{
    CMUTIL_ConfLogger_Internal *cli = (CMUTIL_ConfLogger_Internal*)clogger;
    for ( ;level <= CMUTIL_LogLevel_Fatal; level++) {
        CMUTIL_Array *lapndrs = (CMUTIL_Array*)CMUTIL_CALL(
                    cli->lvlapndrs, GetAt, level);
        CMUTIL_CALL(lapndrs, Add, appender);
    }
}

CMUTIL_STATIC CMUTIL_ConfLogger *CMUTIL_LogSystemCreateLogger(
        CMUTIL_LogSystem *lsys, const char *name, CMUTIL_LogLevel level,
        CMUTIL_Bool additivity)
{
    CMUTIL_Array *lvllog;
    int i, lvllen = CMUTIL_LogLevel_Fatal + 1;
    CMUTIL_LogSystem_Internal *ilsys = (CMUTIL_LogSystem_Internal*)lsys;
    CMUTIL_ConfLogger_Internal *res =
            ilsys->memst->Alloc(sizeof(CMUTIL_ConfLogger_Internal));
    memset(res, 0x0, sizeof(CMUTIL_ConfLogger_Internal));
    if (!name) name = "";
    res->name = ilsys->memst->Strdup(name);
    res->level = level;
    res->additivity = additivity;
    res->lsys = ilsys;
    res->lvlapndrs = CMUTIL_ArrayCreateInternal(
                ilsys->memst, lvllen, NULL, CMUTIL_DoubleArrayDestroyer);
    for (i=0; i<lvllen; i++) {
        CMUTIL_CALL(res->lvlapndrs, Add, CMUTIL_ArrayCreateInternal(
                        ilsys->memst, 3, CMUTIL_LogAppenderComparator, NULL));
    }
    CMUTIL_CALL(ilsys->cloggers, Add, res);
    for (i=level; i<lvllen; i++) {
        lvllog = CMUTIL_CALL(ilsys->levelloggers, GetAt, i);
        CMUTIL_CALL(lvllog, Add, res);
    }
    res->base.AddAppender = CMUTIL_ConfLoggerAddAppender;
    res->Log = CMUTIL_ConfLoggerLog;
    res->Destroy = CMUTIL_ConfLoggerDestroy;
    return (CMUTIL_ConfLogger*)res;
}

CMUTIL_STATIC int CMUTIL_LoggerRefComparator(const void *a, const void *b)
{
    CMUTIL_ConfLogger_Internal *ca = (CMUTIL_ConfLogger_Internal*)a;
    CMUTIL_ConfLogger_Internal *cb = (CMUTIL_ConfLogger_Internal*)b;
    size_t sa = strlen(ca->name), sb = strlen(cb->name);
    // longer name is first.
    if (sa < sb) return 1;
    else if (sa > sb) return -1;
    return 0;
}

CMUTIL_STATIC void CMUTIL_LoggerLogEx(
        CMUTIL_Logger *logger, CMUTIL_LogLevel level,
        const char *file, int line, CMUTIL_Bool printStack,
        const char *fmt, ...)
{
    int i;
    CMUTIL_Logger_Internal *il = (CMUTIL_Logger_Internal*)logger;
    CMUTIL_String *logmsg;
    CMUTIL_StringArray *stack = NULL;
    CMUTIL_StackWalker *sw = NULL;
    va_list args;
    if (level < il->minlevel) return;
    logmsg = CMUTIL_StringCreateInternal(il->memst, 128, NULL);
    va_start(args, fmt);
    CMUTIL_CALL(logmsg, AddVPrint, fmt, args);
    va_end(args);
    // print stack
    if (printStack) {
        sw = CMUTIL_StackWalkerCreateInternal(il->memst);
        stack = CMUTIL_CALL(sw, GetStack, 1);
    }
    for (i=0; i<CMUTIL_CALL(il->logrefs, GetSize); i++) {
        CMUTIL_ConfLogger_Internal *cli = (CMUTIL_ConfLogger_Internal*)
                CMUTIL_CALL(il->logrefs, GetAt, i);
        if (cli->level <= level) {
            if (CMUTIL_CALL(cli, Log, il, level, stack, file, line, logmsg))
                if (!cli->additivity) break;
        }
    }
    CMUTIL_CALL(logmsg, Destroy);
    if (sw) CMUTIL_CALL(sw, Destroy);
    if (stack) CMUTIL_CALL(stack, Destroy);
}

CMUTIL_STATIC void CMUTIL_LoggerDestroy(CMUTIL_Logger_Internal *logger)
{
    CMUTIL_Logger_Internal *il = logger;
    if (il) {
        if (il->fullname)
            CMUTIL_CALL(il->fullname, Destroy);
        if (il->namepath)
            CMUTIL_CALL(il->namepath, Destroy);
        if (il->logrefs)
            CMUTIL_CALL(il->logrefs, Destroy);
        il->memst->Free(il);
    }
}

CMUTIL_STATIC CMUTIL_Logger *CMUTIL_LogSystemGetLogger(
        CMUTIL_LogSystem *lsys, const char *name)
{
    CMUTIL_LogSystem_Internal *ilsys = (CMUTIL_LogSystem_Internal*)lsys;
    CMUTIL_Logger_Internal *res;
    CMUTIL_Logger_Internal q;
    q.fullname = CMUTIL_StringCreateInternal(ilsys->memst, 20, name);
    res = (CMUTIL_Logger_Internal*)CMUTIL_CALL(ilsys->loggers, Find, &q, NULL);
    if (res == NULL) {
        int i;
        // create logger
        res = ilsys->memst->Alloc(sizeof(CMUTIL_Logger_Internal));
        memset(res, 0x0, sizeof(CMUTIL_Logger_Internal));
        res->memst = ilsys->memst;
        res->fullname = q.fullname;
        res->namepath = CMUTIL_StringSplitInternal(ilsys->memst, name, ".");
        res->logrefs = CMUTIL_ArrayCreateInternal(
                    ilsys->memst, 3, CMUTIL_LoggerRefComparator, NULL);
        res->minlevel = CMUTIL_LogLevel_Fatal;
        for (i=0; i<CMUTIL_CALL(ilsys->cloggers, GetSize); i++) {
            CMUTIL_ConfLogger_Internal *cl = (CMUTIL_ConfLogger_Internal*)
                    CMUTIL_CALL(ilsys->cloggers, GetAt, i);
            // adding root logger and parent loggers
            if (strlen(cl->name) == 0 || strstr(name, cl->name) == 0) {
                CMUTIL_CALL(res->logrefs, Add, cl);
                if (res->minlevel > cl->level)
                    res->minlevel = cl->level;
            }
        }
        // logex and destroyer
        res->base.LogEx = CMUTIL_LoggerLogEx;
        res->Destroy = CMUTIL_LoggerDestroy;
        CMUTIL_CALL(ilsys->loggers, Add, res);
    } else {
        CMUTIL_CALL(q.fullname, Destroy);
    }
    return (CMUTIL_Logger*)res;
}

CMUTIL_STATIC void CMUTIL_LogAppenderDestroyer(void *data)
{
    CMUTIL_LogAppender *ap = (CMUTIL_LogAppender*)data;
    CMUTIL_CALL(ap, Destroy);
}

CMUTIL_STATIC int CMUTIL_ConfLoggerComparator(const void *a, const void *b)
{
    CMUTIL_ConfLogger_Internal *acli = (CMUTIL_ConfLogger_Internal*)a;
    CMUTIL_ConfLogger_Internal *bcli = (CMUTIL_ConfLogger_Internal*)b;
    return strcmp(acli->name, bcli->name);
}

CMUTIL_STATIC void CMUTIL_ConfLoggerDestroyer(void *data)
{
    CMUTIL_ConfLogger_Internal *ap = (CMUTIL_ConfLogger_Internal*)data;
    CMUTIL_CALL(ap, Destroy);
}

CMUTIL_STATIC int CMUTIL_LoggerComparator(const void *a, const void *b)
{
    CMUTIL_Logger_Internal *al = (CMUTIL_Logger_Internal*)a;
    CMUTIL_Logger_Internal *bl = (CMUTIL_Logger_Internal*)b;
    return strcmp(
                CMUTIL_CALL(al->fullname, GetCString),
                CMUTIL_CALL(bl->fullname, GetCString));
}

CMUTIL_STATIC void CMUTIL_LoggerDestroyer(void *data)
{
    CMUTIL_Logger_Internal *ap = (CMUTIL_Logger_Internal*)data;
    CMUTIL_CALL(ap, Destroy);
}

CMUTIL_STATIC void CMUTIL_LogSystemDestroy(CMUTIL_LogSystem *lsys)
{
    CMUTIL_LogSystem_Internal *ilsys = (CMUTIL_LogSystem_Internal*)lsys;
    if (ilsys) {
        if (ilsys->appenders) CMUTIL_CALL(ilsys->appenders, Destroy);
        if (ilsys->cloggers) CMUTIL_CALL(ilsys->cloggers, Destroy);
        if (ilsys->levelloggers) CMUTIL_CALL(ilsys->levelloggers, Destroy);
        if (ilsys->loggers) CMUTIL_CALL(ilsys->loggers, Destroy);
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
                memst, 256, CMUTIL_False, CMUTIL_LogAppenderDestroyer);
    res->cloggers = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_ConfLoggerComparator,
                CMUTIL_ConfLoggerDestroyer);
    res->levelloggers = CMUTIL_ArrayCreateInternal(
                memst, CMUTIL_LogLevel_Fatal+1, NULL,
                CMUTIL_DoubleArrayDestroyer);
    for (i=0; i<(CMUTIL_LogLevel_Fatal+1); i++)
        CMUTIL_CALL(res->levelloggers, Add, CMUTIL_ArrayCreateInternal(
                        memst, 3, CMUTIL_ConfLoggerComparator, NULL));
    res->loggers = CMUTIL_ArrayCreateInternal(
                memst, 50, CMUTIL_LoggerComparator,
                CMUTIL_LoggerDestroyer);

    return (CMUTIL_LogSystem*)res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemCreate()
{
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Lock);
    if (__cmutil_logsystem) {
        CMUTIL_CALL(g_cmutil_logsystem_mutex, Destroy);
        __cmutil_logsystem = NULL;
    }
    return CMUTIL_LogSystemCreateInternal(CMUTIL_GetMem());
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Unlock);
}

CMUTIL_STATIC void CMUTIL_LogConfigClean(CMUTIL_Json *json)
{
    int i;
    switch (CMUTIL_CALL(json, GetType)) {
    case CMUTIL_JsonTypeObject: {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMUTIL_StringArray *keys = CMUTIL_CALL(jobj, GetKeys);
        for (i=0; i<CMUTIL_CALL(keys, GetSize); i++) {
            CMUTIL_String *str = CMUTIL_CALL(keys, GetAt, i);
            const char *key = CMUTIL_CALL(str, GetCString);
            CMUTIL_Json *item = CMUTIL_CALL(jobj, Remove, key);
            // change internal buffer to lowercase
            CMUTIL_CALL(str, SelfToLower);
            // change item recursively.
            CMUTIL_LogConfigClean(item);
            // 'key' is changed to lowercase already.
            // so just put with it.
            CMUTIL_CALL(jobj, Put, key, item);
        }
        CMUTIL_CALL(keys, Destroy);
        break;
    }
    case CMUTIL_JsonTypeArray: {
        CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)json;
        for (i=0; i<CMUTIL_CALL(jarr, GetSize); i++)
            // change item recursively.
            CMUTIL_LogConfigClean(CMUTIL_CALL(jarr, Get, i));
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
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Lock);
    // create default logsystem
    CMUTIL_LogAppender *conapndr = CMUTIL_LogConsoleAppenderCreateInternal(
                memst, "__CONSOL", CMUITL_LOG_PATTERN_DEFAULT);
    CMUTIL_ConfLogger *rootlogger;
    CMUTIL_LogSystem *res = CMUTIL_LogSystemCreateInternal(memst);
    rootlogger = CMUTIL_CALL(res, CreateLogger, NULL, CMUTIL_LogLevel_Warn,
                          CMUTIL_True);
    CMUTIL_CALL(rootlogger, AddAppender, conapndr, CMUTIL_LogLevel_Warn);
    __cmutil_logsystem = (CMUTIL_LogSystem*)res;
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Unlock);

    CMLogWarn("logging system not initialized or invalid configuration. "
              "using default configuraiton.");
    return res;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogSystemProcessOneAppender(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL, *spattern = NULL;
    CMUTIL_LogAppender *appender = NULL;
    CMUTIL_Bool async = CMUTIL_False;
    int64 asyncBufsz = 256;

    if (CMUTIL_CALL(json, GetType) != CMUTIL_JsonTypeObject) {
        printf("invalid configuration structure.\n");
        return CMUTIL_False;
    }

    if (CMUTIL_CALL(jobj, Get, "name")) {
        sname = CMUTIL_CALL(jobj, GetCString, "name");
    } else {
        printf("name must be specified in any appender.\n");
        return CMUTIL_False;
    }
    if (CMUTIL_CALL(jobj, Get, "pattern")) {
        spattern = CMUTIL_CALL(jobj, GetCString, "pattern");
    } else {
        spattern = CMUITL_LOG_PATTERN_DEFAULT;
    }
    if (CMUTIL_CALL(jobj, Get, "async")) {
        async = CMUTIL_CALL(jobj, GetBoolean, "async");
    }
    if (CMUTIL_CALL(jobj, Get, "asyncbuffersize")) {
        asyncBufsz = CMUTIL_CALL(jobj, GetLong, "asyncbuffersize");
        if (asyncBufsz < 10) {
            printf("'asyncBufferSize' too small, must be greater than 10\n");
            return CMUTIL_False;
        }
    }

    if (strcmp("console", stype) == 0) {
        appender = CMUTIL_LogConsoleAppenderCreateInternal(
                    lsys->memst, sname, spattern);
    } else if (strcmp("file", stype) == 0) {
        const char *filename = NULL;
        if (CMUTIL_CALL(jobj, Get, "filename")) {
            filename = CMUTIL_CALL(jobj, GetCString, "filename");
        } else {
            printf("fileName must be specified in File type appender.\n");
            return CMUTIL_False;
        }
        appender = CMUTIL_LogFileAppenderCreateInternal(
                    lsys->memst, sname, filename, spattern);
    } else if (strcmp("rollingfile", stype) == 0) {
        char deffpattern[1024];
        const char *filename = NULL, *filepattern = NULL;
        CMUTIL_LogTerm rollterm = CMUTIL_LogTerm_Date;
        if (CMUTIL_CALL(jobj, Get, "filename")) {
            filename = CMUTIL_CALL(jobj, GetCString, "filename");
        } else {
            printf("fileName must be specified in RollingFile "
                   "type appender.\n");
            return CMUTIL_False;
        }
        if (CMUTIL_CALL(jobj, Get, "rollterm")) {
            const char *sterm = CMUTIL_CALL(jobj, GetCString, "rollterm");
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
        if (CMUTIL_CALL(jobj, Get, "filepattern")) {
            filepattern = CMUTIL_CALL(jobj, GetCString, "filepattern");
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
        if (CMUTIL_CALL(jobj, Get, "listenport")) {
            port = (int)CMUTIL_CALL(jobj, GetLong, "listenport");
        } else {
            printf("listenPort must be specified in Socket type appender.\n");
            return CMUTIL_False;
        }
        if (CMUTIL_CALL(jobj, Get, "accepthost")) {
            shost = CMUTIL_CALL(jobj, GetCString, "accepthost");
        }
        appender = CMUTIL_LogSocketAppenderCreateInternal(
                    lsys->memst, sname, shost, port, spattern);
    }

    if (appender) {
        if (async) CMUTIL_CALL(appender, SetAsync, (int)asyncBufsz);
        CMUTIL_CALL(&(lsys->base), AddAppender, appender);
        return CMUTIL_True;
    } else {
        printf("unknown appender type '%s'.\n", stype);
        return CMUTIL_False;
    }
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogSystemAddAppenderToLogger(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_ConfLogger *clogger,
        const char *apndr, CMUTIL_LogLevel level)
{
    CMUTIL_LogAppender *appender = CMUTIL_CALL(lsys->appenders, Get, apndr);
    if (!appender) {
        printf("appender '%s' not found in system.\n", apndr);
        return CMUTIL_False;
    }
    CMUTIL_CALL(clogger, AddAppender, appender, level);
    return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogStringToLevel(
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
        return CMUTIL_False;
    return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogSystemProcessAppenderRef(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_ConfLogger *clogger,
        CMUTIL_Json *json, CMUTIL_LogLevel level)
{
    if (CMUTIL_CALL(json, GetType) == CMUTIL_JsonTypeValue) {
        CMUTIL_JsonValue *jval = (CMUTIL_JsonValue*)json;
        const char *sval = CMUTIL_CALL(jval, GetCString);
        return CMUTIL_LogSystemAddAppenderToLogger(
                    lsys, clogger, sval, level);
    } else {
        CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
        CMUTIL_LogLevel alevel = level;
        const char *ref = NULL;
        if (CMUTIL_CALL(jobj, Get, "ref")) {
            ref = CMUTIL_CALL(jobj, GetCString, "ref");
        } else {
            printf("ref must be specified in AppenderRef.\n");
            return CMUTIL_False;
        }
        if (CMUTIL_CALL(jobj, Get, "level")) {
            const char *salevel = CMUTIL_CALL(jobj, GetCString, "level");
            if (!CMUTIL_LogStringToLevel(salevel, &alevel)) {
                printf("invalid level in AppenderRef.\n");
                return CMUTIL_False;
            }
        }
        return CMUTIL_LogSystemAddAppenderToLogger(
                    lsys, clogger, ref, alevel);
    }
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogSystemProcessOneLogger(
        CMUTIL_LogSystem_Internal *lsys, const char *stype, CMUTIL_Json *json)
{
    CMUTIL_Bool res = CMUTIL_True;
    CMUTIL_JsonObject *jobj = (CMUTIL_JsonObject*)json;
    const char *sname = NULL;
    CMUTIL_ConfLogger *clogger = NULL;
    CMUTIL_Bool additivity = CMUTIL_True;
    CMUTIL_LogLevel level = CMUTIL_LogLevel_Error;
    CMUTIL_Json *apref = NULL;

    if (CMUTIL_CALL(json, GetType) != CMUTIL_JsonTypeObject) {
        printf("invalid configuration structure.\n");
        return CMUTIL_False;
    }

    if (CMUTIL_CALL(jobj, Get, "name")) {
        CMUTIL_String *name = CMUTIL_CALL(jobj, GetString, "name");
        sname = CMUTIL_CALL(name, GetCString);
        if (*sname == 0x0 && strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            return CMUTIL_False;
        }
    } else {
        if (strcmp(stype, "root") != 0) {
            printf("Name must be specified except root logger.\n");
            return CMUTIL_False;
        }
    }
    if (strcmp(stype, "root") != 0) sname = "";
    if (CMUTIL_CALL(jobj, Get, "level")) {
        const char *slevel = CMUTIL_CALL(jobj, GetCString, "level");
        if (!CMUTIL_LogStringToLevel(slevel, &level)) {
            printf("invalid log level in logger.\n");
            return CMUTIL_False;
        }
    } else {
        printf("level must be specified in logger.\n");
        return CMUTIL_False;
    }
    if (CMUTIL_CALL(jobj, Get, "additivity")) {
        additivity = CMUTIL_CALL(jobj, GetBoolean, "additivity");
    }

    clogger = CMUTIL_CALL(
                &(lsys->base), CreateLogger, sname, level, additivity);
    apref = CMUTIL_CALL(jobj, Get, "appenderref");
    if (apref) {
        switch (CMUTIL_CALL(apref, GetType)) {
        case CMUTIL_JsonTypeArray: {
            CMUTIL_JsonArray *jarr = (CMUTIL_JsonArray*)apref;
            int i;
            for (i=0; i<CMUTIL_CALL(jarr, GetSize); i++) {
                CMUTIL_Json *item = CMUTIL_CALL(jarr, Get, i);
                CMUTIL_JsonType type = CMUTIL_CALL(item, GetType);
                if (type == CMUTIL_JsonTypeArray) {
                    printf("invalid appender reference in logger.\n");
                    return CMUTIL_False;
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
    return CMUTIL_True;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_LogSystemProcessItems(
        CMUTIL_LogSystem_Internal *lsys, CMUTIL_Json *json,
        CMUTIL_Bool(*oneprocf)(
            CMUTIL_LogSystem_Internal*, const char *, CMUTIL_Json*))
{
    CMUTIL_Bool res = CMUTIL_False;
    int i;
    CMUTIL_JsonArray *jarr = NULL;
    if (CMUTIL_CALL(json, GetType) != CMUTIL_JsonTypeArray) {
        printf("invalid configuration structure.\n");
        goto ENDPOINT;
    }
    jarr = (CMUTIL_JsonArray*)json;
    for (i=0; i<CMUTIL_CALL(jarr, GetSize); i++) {
        CMUTIL_Json *item = CMUTIL_CALL(jarr, Get, i);
        CMUTIL_JsonObject *oitem = NULL;
        CMUTIL_String *type = NULL;
        const char *stype = NULL;
        if (CMUTIL_CALL(item, GetType) != CMUTIL_JsonTypeObject) {
            printf("invalid configuration structure.\n");
            goto ENDPOINT;
        }
        oitem = (CMUTIL_JsonObject*)item;
        type = CMUTIL_CALL(oitem, GetString, "type");
        CMUTIL_CALL(type, SelfToLower);
        stype = CMUTIL_CALL(type, GetCString);

        if (!oneprocf(lsys, stype, item)) {
            printf("processing one item failed.\n");
            goto ENDPOINT;
        }
    }
    res = CMUTIL_True;
ENDPOINT:
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureInternal(
        CMUTIL_Mem *memst, CMUTIL_Json *json)
{
    CMUTIL_LogSystem *lsys;
    CMUTIL_JsonObject *config = (CMUTIL_JsonObject*)json;
    CMUTIL_Json *appenders, *loggers;
    CMUTIL_Bool succeeded = CMUTIL_False;

    // all keys to lowercase for case insensitive.
    CMUTIL_LogConfigClean(json);
    if (CMUTIL_CALL(config, Get, "configuration"))
        config = (CMUTIL_JsonObject*)CMUTIL_CALL(config, Get, "configuration");
    appenders = CMUTIL_CALL(config, Get, "appenders");
    loggers = CMUTIL_CALL(config, Get, "loggers");

    if (appenders && loggers) {
        CMUTIL_CALL(g_cmutil_logsystem_mutex, Lock);
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
            if (lsys) CMUTIL_CALL(lsys, Destroy);
        }
        CMUTIL_CALL(g_cmutil_logsystem_mutex, Unlock);
    }
    if (!succeeded)
        lsys = CMUTIL_LogSystemConfigureDefault(memst);
    return lsys;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJsonInternal(
        CMUTIL_Mem *memst, const char *jsonfile)
{
    CMUTIL_File *f = CMUTIL_FileCreateInternal(memst, jsonfile);
    CMUTIL_String *jsonstr = CMUTIL_CALL(f, GetContents);
    CMUTIL_Json *json = CMUTIL_JsonParseInternal(memst, jsonstr, CMUTIL_True);
    CMUTIL_LogSystem *res = CMUTIL_LogSystemConfigureInternal(memst, json);
    CMUTIL_CALL(json, Destroy);
    CMUTIL_CALL(jsonstr, Destroy);
    CMUTIL_CALL(f, Destroy);
    return res;
}

CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(const char *jsonfile)
{
    return CMUTIL_LogSystemConfigureFomJsonInternal(
                CMUTIL_GetMem(), jsonfile);
}

static CMUTIL_Bool g_cmutil_logsystem_initializing = CMUTIL_False;

CMUTIL_LogSystem *CMUTIL_LogSystemGetInternal(CMUTIL_Mem *memst)
{
    if (__cmutil_logsystem) return __cmutil_logsystem;

    CMUTIL_CALL(g_cmutil_logsystem_mutex, Lock);
    if (g_cmutil_logsystem_initializing) return NULL;
    g_cmutil_logsystem_initializing = CMUTIL_True;
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Unlock);

    CMUTIL_CALL(g_cmutil_logsystem_mutex, Lock);
    if (__cmutil_logsystem == NULL) {
        // try to read cmutil_log.json
        char *conf = getenv("CMUTIL_LOG_CONFIG");
        if (!conf) conf = "cmutil_log.json";
        CMUTIL_LogSystemConfigureFomJsonInternal(memst, conf);
    }
    CMUTIL_CALL(g_cmutil_logsystem_mutex, Unlock);
    g_cmutil_logsystem_initializing =
            __cmutil_logsystem == NULL? CMUTIL_True:CMUTIL_False;
    return __cmutil_logsystem;
}

CMUTIL_LogSystem *CMUTIL_LogSystemGet()
{
    return CMUTIL_LogSystemGetInternal(CMUTIL_GetMem());
}
