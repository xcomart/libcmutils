
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "platforms.h"

#define CMUTIL_EXPORT
#include "libcmutils.h"

#include "pattern.h"

#if defined(DEBUG)
# define CMUTIL_STATIC
#else
# define CMUTIL_STATIC	static
#endif

#define SPACES      " \r\n\t"

#if defined(ARCH32)
typedef unsigned int	CMUTIL_PointDiff;
#else
typedef uint64          CMUTIL_PointDiff;
#endif

#if defined(_MSC_VER)
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

typedef struct CMUTIL_Mem_st CMUTIL_Mem_st;

void CMUTIL_XmlInit();
void CMUTIL_XmlClear();
void CMUTIL_LogInit();
void CMUTIL_LogClear();
void CMUTIL_ThreadInit();
void CMUTIL_ThreadClear();
void CMUTIL_CallStackInit();
void CMUTIL_CallStackClear();
void CMUTIL_NetworkInit();
void CMUTIL_NetworkClear();
void CMUTIL_MemDebugInit(CMUTIL_MemOper memoper);
void CMUTIL_MemDebugClear();


CMUTIL_Array *CMUTIL_ArrayCreateInternal(
        CMUTIL_Mem_st *mem,
        int initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*));

CMUTIL_StackWalker *CMUTIL_StackWalkerCreateInternal(CMUTIL_Mem_st *memst);

CMUTIL_Cond *CMUTIL_CondCreateInternal(
        CMUTIL_Mem_st *memst, CMUTIL_Bool manual_reset);
CMUTIL_Mutex *CMUTIL_MutexCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_Thread *CMUTIL_ThreadCreateInternal(
        CMUTIL_Mem_st *memst, void*(*proc)(void*), void *udata,
        const char *name);
CMUTIL_Semaphore *CMUTIL_SemaphoreCreateInternal(
        CMUTIL_Mem_st *memst, int initcnt);
CMUTIL_RWLock *CMUTIL_RWLockCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_Timer *CMUTIL_TimerCreateInternal(
        CMUTIL_Mem_st *memst, long precision, int threads);

CMUTIL_Config *CMUTIL_ConfigCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_Config *CMUTIL_ConfigLoadInternal(
        CMUTIL_Mem_st *memst, const char *fconf);

CMUTIL_List *CMUTIL_ListCreateInternal(
        CMUTIL_Mem_st *memst, void(*freecb)(void*));

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreateInternal(
        CMUTIL_Mem_st *memst, const char *name, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreateInternal(
        CMUTIL_Mem_st *memst, const char *name,
        const char *fpath, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreateInternal(
        CMUTIL_Mem_st *memst, const char *name,
        const char *fpath, CMUTIL_LogTerm logterm,
        const char *rollpath, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreateInternal(
        CMUTIL_Mem_st *memst, const char *name, const char *accept_host,
        int listen_port, const char *pattern);
CMUTIL_LogSystem *CMUTIL_LogSystemCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureInternal(
        CMUTIL_Mem_st *memst, CMUTIL_Json *json);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFromXmlInternal(
        CMUTIL_Mem_st *memst, const char *xmlfile);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJsonInternal(
        CMUTIL_Mem_st *memst, const char *jsonfile);
CMUTIL_LogSystem *CMUTIL_LogSystemGetInternal(CMUTIL_Mem_st *memst);

CMUTIL_Map *CMUTIL_MapCreateInternal(
        CMUTIL_Mem_st *memst, int bucketsize, void(*freecb)(void*));

CMUTIL_JsonObject *CMUTIL_JsonObjectCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_JsonArray *CMUTIL_JsonArrayCreateInternal(CMUTIL_Mem_st *memst);
CMUTIL_Json *CMUTIL_JsonParseInternal(
        CMUTIL_Mem_st *memst, CMUTIL_String *jsonstr, CMUTIL_Bool silent);

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLenInternal(
        CMUTIL_Mem_st *memst,
        CMUTIL_XmlNodeKind type, const char *tagname, int namelen);
CMUTIL_XmlNode *CMUTIL_XmlNodeCreateInternal(
        CMUTIL_Mem_st *memst, CMUTIL_XmlNodeKind type, const char *tagname);
CMUTIL_XmlNode *CMUTIL_XmlParseStringInternal(
        CMUTIL_Mem_st *memst, const char *xmlstr, int len);
CMUTIL_XmlNode *CMUTIL_XmlParseInternal(
        CMUTIL_Mem_st *memst, CMUTIL_String *str);
CMUTIL_XmlNode *CMUTIL_XmlParseFileInternal(
        CMUTIL_Mem_st *memst, const char *fpath);

CMUTIL_Socket *CMUTIL_SocketConnectInternal(
        CMUTIL_Mem_st *memst, const char *host, int port, long timeout,
        CMUTIL_Bool silent);
CMUTIL_ServerSocket *CMUTIL_ServerSocketCreateInternal(
        CMUTIL_Mem_st *memst, const char *host, int port, int qcnt,
        CMUTIL_Bool silent);

CMUTIL_Bool CMUTIL_PatternIsValid(const char *pat);
CMUTIL_Bool CMUTIL_PatternMatch(const char *pat, const char *fname);
CMUTIL_Bool CMUTIL_PatternMatchN(const char *pat, const char *fname);


CMUTIL_Pool *CMUTIL_PoolCreateInternal(
        CMUTIL_Mem_st *memst,
        int initcnt, int maxcnt,
        void *(*createproc)(void *),
        void (*destroyproc)(void *, void *),
        CMUTIL_Bool (*testproc)(void *, void *),
        long pinginterval,
        CMUTIL_Bool testonborrow,
        void *udata, CMUTIL_Timer *timer);

CMUTIL_String *CMUTIL_StringCreateInternal(
        CMUTIL_Mem_st *memst,
        int initcapacity,
        const char *initcontent);
CMUTIL_StringArray *CMUTIL_StringArrayCreateInternal(
        CMUTIL_Mem_st *memst, int initcapacity);
CMUTIL_CSConv *CMUTIL_CSConvCreateInternal(
        CMUTIL_Mem_st *memst, const char *fromcs, const char *tocs);
CMUTIL_StringArray *CMUTIL_StringSplitInternal(
        CMUTIL_Mem_st *memst, const char *haystack, const char *needle);

CMUTIL_Library *CMUTIL_LibraryCreateInternal(
        CMUTIL_Mem_st *memst, const char *path);
CMUTIL_File *CMUTIL_FileCreateInternal(CMUTIL_Mem_st *memst, const char *path);

#endif // FUNCTIONS_H

