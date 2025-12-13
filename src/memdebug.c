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
#include <math.h>
#include <assert.h>
#include <time.h>

#if defined(_MSC_VER)
# define STRDUP _strdup
#else
# define STRDUP strdup
#endif

#define MEM_BLOCK_SZ 45


static CMUTIL_Mem g_cmutil_memdebug_system = {
    malloc,
    calloc,
    realloc,
    STRDUP,
    free
};

static CMUTIL_Mem *__CMUTIL_Mem = &g_cmutil_memdebug_system;

static CMMemOper g_cmutil_memoper = CMMemSystem;
static CMUTIL_StackWalker *g_cmutil_memstackwalker = NULL;
static CMUTIL_Mutex *g_cmutil_memlog_mutex = NULL;


CMUTIL_STATIC FILE *CMUTIL_MemGetFP()
{
    static char buf[2048];
    static CMBool havepath = CMFalse;
    static int rcnt = 0;
    FILE *res = NULL;

    if (g_cmutil_memlog_mutex) CMCall(g_cmutil_memlog_mutex, Lock);
    if (!havepath) {
        ssize_t sz;
        struct tm currtm;
        struct timeval tv;
        char dtbuf[32];

        gettimeofday(&tv, NULL);
        localtime_r((time_t*)&(tv.tv_sec), &currtm);
#if defined(LINUX)
        sz = readlink("/proc/self/exe", buf, sizeof(buf));
#elif defined(SUNOS)
        sz = readlink("/proc/self/path/a.out", buf, sizeof(buf));
#elif defined(BSD) || defined(APPLE)
        const char * appname = getprogname();
        buf[0] = 0x0;
        strcat(buf, appname);
        sz = (ssize_t)strlen(buf);
#elif defined(MSWIN)
        sz = (size_t)GetModuleFileName(NULL, buf, sizeof(buf));
#else
        // unknown platform just save it current dir
        sz = 0;
#endif
        buf[sz] = 0x0;
        strftime(dtbuf, sizeof(dtbuf), "%Y%m%d_%H%M%S", &currtm);
        strcat(buf, "_mem_log_");
        strcat(buf, dtbuf);
        strcat(buf, ".txt");

        printf("memory log will be stored at '%s'.\n", buf);

        havepath = CMTrue;
    }

RETRY:
    res = fopen(buf, "a");
    if (res == NULL) {
        if (rcnt == 0) {
            sprintf(buf, "mem_log.txt");
            rcnt++;
            goto RETRY;
        }
        res = stdout;
    }
    if (g_cmutil_memlog_mutex) CMCall(g_cmutil_memlog_mutex, Unlock);
    return res;
}

#define CMUTIL_MEMLOG_BUFSZ 4096

CMUTIL_STATIC void CMUTIL_MemLogInternal(
        FILE *fp, const char *format, va_list args)
{
    char datebuf[25];
    char formatbuf[256];
    struct tm tm_temp;
    struct tm *tm_time = &tm_temp;
    time_t curTime;
    char logstr[CMUTIL_MEMLOG_BUFSZ+10] = {0,};

    curTime = time(NULL);
    localtime_r(&curTime, tm_time);
    strftime(datebuf, 25, "%Y%m%d %H%M%S", tm_time);

    sprintf(formatbuf, "%s %s"S_CRLF, datebuf, format);
    vsnprintf(logstr, CMUTIL_MEMLOG_BUFSZ, formatbuf, args);
    strcpy(logstr+CMUTIL_MEMLOG_BUFSZ-2, "..."S_CRLF);

    fprintf(fp, "%s", logstr);
}

CMUTIL_STATIC void CMUTIL_MemLog(const char *fmt, ...)
{
    va_list vargs;
    FILE *fp = CMUTIL_MemGetFP();
    va_start(vargs, fmt);
    CMUTIL_MemLogInternal(fp, fmt, vargs);
    va_end(vargs);
    if (fp != stdout)
        fclose(fp);
}

CMUTIL_STATIC void CMUTIL_MemLogWithStack(const char *fmt, ...)
{
    va_list vargs;
    FILE *fp = CMUTIL_MemGetFP();
    char prebuf[CMUTIL_MEMLOG_BUFSZ];
    CMUTIL_String *buffer = CMUTIL_StringCreateInternal(
                &g_cmutil_memdebug_system, 256, NULL);
    CMCall(g_cmutil_memstackwalker, PrintStack, buffer, 0);
    va_start(vargs, fmt);
    vsnprintf(prebuf, CMUTIL_MEMLOG_BUFSZ, fmt, vargs);
    va_end(vargs);
    CMUTIL_MemLog("%s%s"S_CRLF, prebuf, CMCall(buffer, GetCString));
    CMCall(buffer, Destroy);
    if (fp != stdout)
        fclose(fp);
}

typedef struct CMUTIL_MemNode {
    size_t                  size;
    struct CMUTIL_MemNode   *next;
    CMUTIL_String           *stack;
    int                     index;
    unsigned char           flag;
    char                    dummy_padder[3];
} CMUTIL_MemNode;

CMUTIL_STATIC void CMUTIL_MemNodeDestroy(void *data)
{
    CMUTIL_MemNode *node = data;
    if (node->stack)
        CMCall(node->stack, Destroy);
    free(data);
}

CMUTIL_STATIC int CMUTIL_MemRcyComparator(const void *a, const void *b)
{
    // memory address comparison
    if (a > b)
        return 1;
    if (a < b)
        return -1;
    return 0;
}

typedef struct CMUTIL_MemRcyList CMUTIL_MemRcyList;
static struct CMUTIL_MemRcyList {
    CMUTIL_Mutex    *mutex;
    int             cnt;
    int             avlcnt;
    CMUTIL_MemNode  *head;
    CMUTIL_Array    *used;
    size_t          usedsize;
} g_cmutil_memrcyblocks[MEM_BLOCK_SZ];

CMUTIL_STATIC int CMUTIL_MemRcyIndex(size_t size)
{
    int rval = (int)ceil(log2((double)size));
    return rval < 0? 0:rval;
}

CMUTIL_STATIC void *CMUTIL_MemRcyAlloc(size_t size)
{
    int idx = CMUTIL_MemRcyIndex(size);
    void *res;
    CMUTIL_MemNode *node;
    CMUTIL_MemRcyList *list;
    if (idx >= MEM_BLOCK_SZ) {
        CMUTIL_MemLogWithStack(
                    "*** FATAL - allocating size too big(%ld).", (long)size);
        assert(0);
    }
    list = &g_cmutil_memrcyblocks[idx];
    CMCall(list->mutex, Lock);
    if (list->head) {
        node = list->head;
        res = (uint8_t*)node;
        list->head = node->next;
        list->avlcnt--;
    } else {
        list->cnt++;
        res = malloc(sizeof(CMUTIL_MemNode) + (size_t)(1L << idx) + 1);
        node = (CMUTIL_MemNode*)res;
        node->index = idx;
    }
    CMCall(list->used, Add, node);
    list->usedsize += size;
    CMCall(list->mutex, Unlock);
    node->size = size;
    node->next = NULL;
    node->flag = 0xFF;  // underflow writing checker
    *((uint8_t*)res + sizeof(CMUTIL_MemNode) + size) = 0xFF;    // overflow writing checker
    if (g_cmutil_memoper == CMMemDebug) {
        node->stack = CMUTIL_StringCreateInternal(
                    &g_cmutil_memdebug_system, 256, NULL);
        CMCall(g_cmutil_memstackwalker, PrintStack, node->stack, 0);
    } else
        node->stack = NULL;

    return (uint8_t*)res + sizeof(CMUTIL_MemNode);
}

CMUTIL_STATIC void *CMUTIL_MemRcyCalloc(size_t nmem, size_t size)
{
    size_t totsz = nmem * size;
    void *res = CMUTIL_MemRcyAlloc(totsz);
    memset(res, 0x0, totsz);
    return res;
}

CMUTIL_STATIC CMBool CMUTIL_MemCheckFlow(CMUTIL_MemNode *node)
{
    if (node->flag != 0xFF) {
        if (node->stack) {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory underflow written detected while "
                        "freeing. which allocated from%s"S_CRLF
                        " freeing location is",
                        CMCall(node->stack, GetCString));
        } else {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory underflow written detected while "
                        "freeing.");
        }
        return CMFalse;
    }
    if (*(((unsigned char*)node+sizeof(CMUTIL_MemNode)+node->size)) != 0xFF) {
        if (node->stack) {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory overflow written detected while "
                        "freeing. which allocated from%s"S_CRLF
                        " freeing location is",
                        CMCall(node->stack, GetCString));
        } else {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory overflow written detected while "
                        "freeing.");
        }
        return CMFalse;
    }
    return CMTrue;
}

CMUTIL_STATIC void CMUTIL_MemRcyFree(void *ptr)
{
    CMUTIL_MemNode *node =
            (CMUTIL_MemNode*)(((CMUTIL_MemNode*)ptr) - 1);
    CMUTIL_MemRcyList *list = &g_cmutil_memrcyblocks[node->index];

    CMCall(list->mutex, Lock);
    if (CMCall(list->used, Remove, node) == NULL) {
        CMCall(list->mutex, Unlock);
        CMUTIL_MemLogWithStack(
                    "*** FATAL - not allocated memory to be freed.");
        return;
    }

    if (!CMUTIL_MemCheckFlow(node)) {
        CMCall(list->mutex, Unlock);
        return;
    }

    // clear stack
    if (node->stack) {
        CMCall(node->stack, Destroy);
        node->stack = NULL;
    }


    node->next = list->head;
    list->head = node;
    list->avlcnt++;

    CMCall(list->mutex, Unlock);
}

CMUTIL_STATIC void *CMUTIL_MemRcyRealloc(void *ptr, size_t size)
{
    CMUTIL_MemNode *node =
            (CMUTIL_MemNode*)(((CMUTIL_MemNode*)ptr) - 1);
    int nidx = CMUTIL_MemRcyIndex(size);
    if (node->index >= MEM_BLOCK_SZ) {
        CMUTIL_MemLogWithStack("*** FATAL - memory index out of bound. "
                               "is this memory allocated using CMAlloc?");
        return NULL;
    }
    if (!CMUTIL_MemCheckFlow(node))
        return NULL;
    if (nidx >= MEM_BLOCK_SZ) {
        CMUTIL_MemLogWithStack("*** FATAL - memory index out of bound. "
                               "requested memory too big(%"PRIu64
                               ") to allocate.", (uint64_t)size);
        return NULL;
    }
    if (node->index == nidx) {
        CMUTIL_MemRcyList *list;
        list = &g_cmutil_memrcyblocks[nidx];
        CMCall(list->mutex, Unlock);
        list->usedsize += (size - node->size);
        node->size = size;
        CMCall(list->mutex, Unlock);
        // update stack information
        if (node->stack)
            CMCall(node->stack, Destroy);
        if (g_cmutil_memoper == CMMemDebug) {
            node->stack = CMUTIL_StringCreateInternal(
                        &g_cmutil_memdebug_system, 256, NULL);
            CMCall(g_cmutil_memstackwalker, PrintStack, node->stack, 0);
        } else
            node->stack = NULL;
        *(((uint8_t*)ptr) + size) = 0xFF;
        return ptr;
    } else {
        void *res = CMUTIL_MemRcyAlloc(size);
        memcpy(res, ptr, node->size);
        CMUTIL_MemRcyFree(ptr);
        return res;
    }
}

CMUTIL_STATIC char *CMUTIL_MemRcyStrdup(const char *str)
{
    if (str) {
        size_t size = strlen(str)+1;
        char *res = CMUTIL_MemRcyAlloc(size);
        memcpy(res, str, size);
        return res;
    } else {
        CMUTIL_MemLogWithStack("strdup with null pointer");
        return NULL;
    }
}

static CMUTIL_Mem g_cmutil_memdebug = {
    CMUTIL_MemRcyAlloc,
    CMUTIL_MemRcyCalloc,
    CMUTIL_MemRcyRealloc,
    CMUTIL_MemRcyStrdup,
    CMUTIL_MemRcyFree
};

CMUTIL_STATIC void CMUTIL_MemRcyInit()
{
    int i;
    __CMUTIL_Mem = &g_cmutil_memdebug;
    memset(g_cmutil_memrcyblocks, 0x0, sizeof(g_cmutil_memrcyblocks));
    for (i=0; i<MEM_BLOCK_SZ; i++) {
        g_cmutil_memrcyblocks[i].mutex = CMUTIL_MutexCreateInternal(
                    &g_cmutil_memdebug_system);
        g_cmutil_memrcyblocks[i].used = CMUTIL_ArrayCreateInternal(
                    &g_cmutil_memdebug_system, 10,
                    CMUTIL_MemRcyComparator, CMUTIL_MemNodeDestroy, CMTrue);
    }
    g_cmutil_memstackwalker = CMUTIL_StackWalkerCreateInternal(
                &g_cmutil_memdebug_system);
    g_cmutil_memlog_mutex = CMUTIL_MutexCreateInternal(
                &g_cmutil_memdebug_system);
}

void CMUTIL_MemDebugInit(CMMemOper memoper)
{
    g_cmutil_memoper = memoper;
    switch (memoper) {
    case CMMemSystem:
        // do nothing
        break;
    default:
        CMUTIL_MemRcyInit();
    }
}

CMBool CMUTIL_MemDebugClear()
{
    CMBool res = CMTrue;
    if (g_cmutil_memoper != CMMemSystem) {
        uint32_t i;
        for (i=0; i<MEM_BLOCK_SZ; i++) {
            CMUTIL_MemRcyList *list = &g_cmutil_memrcyblocks[i];
            // TODO: print used items;
            if (CMCall(list->used, GetSize) > 0) {
                CMUTIL_MemLog("*** FATAL - index:%d count:%d total:%ld byte "
                        "memory leak detected.",
                        i, list->cnt - list->avlcnt, list->usedsize);
                if (g_cmutil_memoper == CMMemDebug) {
                    uint32_t j;
                    for (j=0; j<CMCall(list->used, GetSize); j++) {
                        CMUTIL_MemNode *node = (CMUTIL_MemNode*)CMCall(
                                    list->used, GetAt, j);
                        CMUTIL_MemLog("* %dth memory leak. size:%d%s"S_CRLF,
                                      j, node->size,
                                      CMCall(node->stack, GetCString));
                    }
                }
                res = CMFalse;
            }
            CMCall(list->used, Destroy);
            while (list->head) {
                CMUTIL_MemNode *curr = list->head;
                list->head = curr->next;
                CMUTIL_MemNodeDestroy(curr);
            }
            CMCall(list->mutex, Destroy);
        }
        if (g_cmutil_memstackwalker) {
            CMCall(g_cmutil_memstackwalker, Destroy);
            g_cmutil_memstackwalker = NULL;
        }
        if (g_cmutil_memlog_mutex) {
            CMCall(g_cmutil_memlog_mutex, Destroy);
            g_cmutil_memlog_mutex = NULL;
        }
        __CMUTIL_Mem = &g_cmutil_memdebug_system;
        g_cmutil_memoper = CMMemSystem;
    }
    return res;
}

CMUTIL_Mem *CMUTIL_GetMem()
{
    return __CMUTIL_Mem;
}
