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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "platforms.h"

#define CMUTIL_EXPORT
#include "libcmutils.h"

#if defined(DEBUG)
# define CMUTIL_STATIC
#else
# define CMUTIL_STATIC	static
#endif

#define SPACES      " \r\n\t"

#if defined(ARCH32)
typedef unsigned int	CMUTIL_PointDiff;
#else
typedef uint64_t        CMUTIL_PointDiff;
#endif

#if defined(_MSC_VER)
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

typedef struct CMUTIL_Mem CMUTIL_Mem;

void CMUTIL_XmlInit(void);
void CMUTIL_XmlClear(void);
void CMUTIL_LogInit(void);
void CMUTIL_LogClear(void);
void CMUTIL_ThreadInit(void);
void CMUTIL_ThreadClear(void);
void CMUTIL_CallStackInit(void);
void CMUTIL_CallStackClear(void);
void CMUTIL_NetworkInit(void);
void CMUTIL_NetworkClear(void);
void CMUTIL_StringBaseInit(void);
void CMUTIL_StringBaseClear(void);
void CMUTIL_MemDebugInit(CMUTIL_MemOper memoper);
void CMUTIL_MemDebugClear(void);


CMUTIL_Array *CMUTIL_ArrayCreateInternal(
        CMUTIL_Mem *mem,
        size_t initcapacity,
        int(*comparator)(const void*,const void*),
        void(*freecb)(void*),
        CMBool sorted);

CMUTIL_StackWalker *CMUTIL_StackWalkerCreateInternal(
        CMUTIL_Mem *memst);

CMUTIL_Cond *CMUTIL_CondCreateInternal(
        CMUTIL_Mem *memst,
        CMBool manual_reset);
CMUTIL_Mutex *CMUTIL_MutexCreateInternal(
        CMUTIL_Mem *memst);
CMUTIL_Thread *CMUTIL_ThreadCreateInternal(
        CMUTIL_Mem *memst,
        void*(*proc)(void*),
        void *udata,
        const char *name);
CMUTIL_Semaphore *CMUTIL_SemaphoreCreateInternal(
        CMUTIL_Mem *memst,
        int initcnt);
CMUTIL_RWLock *CMUTIL_RWLockCreateInternal(
        CMUTIL_Mem *memst);
CMUTIL_Timer *CMUTIL_TimerCreateInternal(
        CMUTIL_Mem *memst,
        long precision,
        int threads);

CMUTIL_Config *CMUTIL_ConfigCreateInternal(CMUTIL_Mem *memst);
CMUTIL_Config *CMUTIL_ConfigLoadInternal(
        CMUTIL_Mem *memst, const char *fconf);

CMUTIL_List *CMUTIL_ListCreateInternal(
        CMUTIL_Mem *memst, void(*freecb)(void*));

CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name,
        const char *fpath, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name,
        const char *fpath, CMUTIL_LogTerm logterm,
        const char *rollpath, const char *pattern);
CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreateInternal(
        CMUTIL_Mem *memst, const char *name, const char *accept_host,
        int listen_port, const char *pattern);
CMUTIL_LogSystem *CMUTIL_LogSystemCreateInternal(CMUTIL_Mem *memst);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureInternal(
        CMUTIL_Mem *memst, CMUTIL_Json *json);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFromXmlInternal(
        CMUTIL_Mem *memst, const char *xmlfile);
CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJsonInternal(
        CMUTIL_Mem *memst, const char *jsonfile);
CMUTIL_LogSystem *CMUTIL_LogSystemGetInternal(CMUTIL_Mem *memst);

CMUTIL_Map *CMUTIL_MapCreateInternal(CMUTIL_Mem *memst, uint32_t bucketsize,
        CMBool isucase, void(*freecb)(void*));

CMUTIL_JsonObject *CMUTIL_JsonObjectCreateInternal(CMUTIL_Mem *memst);
CMUTIL_JsonArray *CMUTIL_JsonArrayCreateInternal(CMUTIL_Mem *memst);
CMUTIL_Json *CMUTIL_JsonParseInternal(
        CMUTIL_Mem *memst, CMUTIL_String *jsonstr, CMBool silent);

CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLenInternal(CMUTIL_Mem *memst,
        CMUTIL_XmlNodeKind type, const char *tagname, size_t namelen);
CMUTIL_XmlNode *CMUTIL_XmlNodeCreateInternal(
        CMUTIL_Mem *memst, CMUTIL_XmlNodeKind type, const char *tagname);
CMUTIL_XmlNode *CMUTIL_XmlParseStringInternal(
        CMUTIL_Mem *memst, const char *xmlstr, size_t len);
CMUTIL_XmlNode *CMUTIL_XmlParseInternal(
        CMUTIL_Mem *memst, CMUTIL_String *str);
CMUTIL_XmlNode *CMUTIL_XmlParseFileInternal(
        CMUTIL_Mem *memst, const char *fpath);

CMUTIL_Socket *CMUTIL_SocketConnectInternal(
        CMUTIL_Mem *memst, const char *host, int port, long timeout,
        CMBool silent);
CMUTIL_Socket *CMUTIL_SSLSocketConnectInternal(
        CMUTIL_Mem *memst,
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout,
        CMBool silent);
CMUTIL_ServerSocket *CMUTIL_ServerSocketCreateInternal(
        CMUTIL_Mem *memst, const char *host, int port, int qcnt,
        CMBool silent);
CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreateInternal(
        CMUTIL_Mem *memst, const char *host, int port, int qcnt,
        const char *cert, const char *key, const char *ca,
        CMBool silent);

CMBool CMUTIL_PatternIsValid(const char *pat);
CMBool CMUTIL_PatternMatch(const char *pat, const char *fname);
CMBool CMUTIL_PatternMatchN(const char *pat, const char *fname);


CMUTIL_Pool *CMUTIL_PoolCreateInternal(
        CMUTIL_Mem *memst,
        int initcnt, int maxcnt,
        void *(*createproc)(void *),
        void (*destroyproc)(void *, void *),
        CMBool (*testproc)(void *, void *),
        long pinginterval,
        CMBool testonborrow,
        void *udata, CMUTIL_Timer *timer);

CMUTIL_String *CMUTIL_StringCreateInternal(
        CMUTIL_Mem *memst,
        size_t initcapacity,
        const char *initcontent);
CMUTIL_StringArray *CMUTIL_StringArrayCreateInternal(
        CMUTIL_Mem *memst, size_t initcapacity);
CMUTIL_CSConv *CMUTIL_CSConvCreateInternal(
        CMUTIL_Mem *memst, const char *fromcs, const char *tocs);
CMUTIL_StringArray *CMUTIL_StringSplitInternal(
        CMUTIL_Mem *memst, const char *haystack, const char *needle);

CMUTIL_Library *CMUTIL_LibraryCreateInternal(
        CMUTIL_Mem *memst, const char *path);
CMUTIL_File *CMUTIL_FileCreateInternal(CMUTIL_Mem *memst, const char *path);

#endif // FUNCTIONS_H

