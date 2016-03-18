
#include "functions.h"
#include <math.h>
#include <assert.h>
#include <time.h>

#if defined(_MSC_VER)
# define STRDUP	_strdup
#else
# define STRDUP strdup
#endif

#define MEM_BLOCK_SZ 45


static CMUTIL_Mem_st g_cmutil_memdebug_system = {
    malloc,
    calloc,
    realloc,
    STRDUP,
    free
};

CMUTIL_Mem_st *__CMUTIL_Mem = &g_cmutil_memdebug_system;

static CMUTIL_MemOper g_cmutil_memoper = CMUTIL_MemSystem;
static CMUTIL_StackWalker *g_cmutil_memstackwalker = NULL;
static CMUTIL_Mutex *g_cmutil_memlog_mutex = NULL;

CMUTIL_STATIC FILE *CMUTIL_MemGetFP()
{
    static char buf[2048];
    static CMUTIL_Bool havepath = CMUTIL_False;
    static int rcnt = 0;
    FILE *res = NULL;

    if (g_cmutil_memlog_mutex) CMUTIL_CALL(g_cmutil_memlog_mutex, Lock);
    if (!havepath) {
        size_t sz;
#if defined(LINUX)
        sz = readlink("/proc/self/exe", buf, sizeof(buf));
#elif defined(SUNOS)
        sz = readlink("/proc/self/path/a.out", buf, sizeof(buf));
#elif defined(BSD)
        sz = readlink("/proc/curproc/file", buf, sizeof(buf));
#elif defined(MSWIN)
        sz = (size_t)GetModuleFileName(NULL, buf, sizeof(buf));
#else
        // unknown platform just save it current dir
        sz = 0;
#endif
        buf[sz] = 0x0;
        strcat(buf, "_mem_log.txt");

        printf("memory log will be stored at '%s'.\n", buf);

        havepath = CMUTIL_True;
    }

RETRY:
    res = fopen(buf, "a");
    if (res == NULL) {
        if (rcnt == 0) {
            sprintf(buf, "mem_log.txt");
            rcnt++;
            goto RETRY;
        } else {
            res = stdout;
        }
    }
    if (g_cmutil_memlog_mutex) CMUTIL_CALL(g_cmutil_memlog_mutex, Unlock);
    return res;
}

#define CMUTIL_MEMLOG_BUFSZ	4096

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
    CMUTIL_CALL(g_cmutil_memstackwalker, PrintStack, buffer, 0);
    va_start(vargs, fmt);
    vsnprintf(prebuf, CMUTIL_MEMLOG_BUFSZ, fmt, vargs);
    va_end(vargs);
    CMUTIL_MemLog("%s%s"S_CRLF, prebuf, CMUTIL_CALL(buffer, GetCString));
    CMUTIL_CALL(buffer, Destroy);
    if (fp != stdout)
        fclose(fp);
}

typedef struct CMUTIL_MemNode {
    int						index;
    size_t					size;
    struct CMUTIL_MemNode   *next;
    unsigned char			flag;
    CMUTIL_String			*stack;
} CMUTIL_MemNode;

CMUTIL_STATIC void CMUTIL_MemNodeDestroy(void *data)
{
    CMUTIL_MemNode *node = data;
    if (node->stack)
        CMUTIL_CALL(node->stack, Destroy);
    free(data);
}

CMUTIL_STATIC int CMUTIL_MemRcyComparator(const void *a, const void *b)
{
    // memory address comparison
    if (a > b)
        return 1;
    else if (a < b)
        return -1;
    else
        return 0;
}

typedef struct CMUTIL_MemRcyList CMUTIL_MemRcyList;
struct CMUTIL_MemRcyList {
    CMUTIL_Mutex	*mutex;
    int				cnt;
    int				avlcnt;
    CMUTIL_MemNode	*head;
    CMUTIL_Array	*used;
    size_t			usedsize;
} g_cmutil_memrcyblocks[MEM_BLOCK_SZ];

CMUTIL_STATIC int CMUTIL_MemRcyIndex(size_t size)
{
    int rval = (int)ceil(log2((double)size));
    return rval < 0? 0:rval;
}

CMUTIL_STATIC void *CMUTIL_MemRcyAlloc(size_t size)
{
    int idx = CMUTIL_MemRcyIndex(size);
    char *res;
    CMUTIL_MemNode *node;
    CMUTIL_MemRcyList *list;
    if (idx >= MEM_BLOCK_SZ) {
        CMUTIL_MemLogWithStack(
                    "*** FATAL - allocating size too big(%ld).", (long)size);
        assert(0);
    }
    list = &g_cmutil_memrcyblocks[idx];
    CMUTIL_CALL(list->mutex, Lock);
    if (list->head) {
        node = list->head;
        res = (char*)node;
        list->head = node->next;
        list->avlcnt--;
    } else {
        list->cnt++;
        res = malloc(sizeof(CMUTIL_MemNode) + (size_t)(1L << idx) + 1);
        node = (CMUTIL_MemNode*)res;
        node->index = idx;
    }
    CMUTIL_CALL(list->used, Add, node);
    list->usedsize += size;
    CMUTIL_CALL(list->mutex, Unlock);
    node->size = size;
    node->next = NULL;
    node->flag = 0xFF;	// underflow writing checker
    *(res + sizeof(CMUTIL_MemNode) + size) = 0xFF;	// overflow writing checker
    if (g_cmutil_memoper == CMUTIL_MemDebug) {
        node->stack = CMUTIL_StringCreateInternal(
                    &g_cmutil_memdebug_system, 256, NULL);
        CMUTIL_CALL(g_cmutil_memstackwalker, PrintStack, node->stack, 0);
    } else
        node->stack = NULL;

    return res + sizeof(CMUTIL_MemNode);
}

CMUTIL_STATIC void *CMUTIL_MemRcyCalloc(size_t nmem, size_t size)
{
    size_t totsz = nmem * size;
    void *res = CMUTIL_MemRcyAlloc(totsz);
    memset(res, 0x0, totsz);
    return res;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_MemCheckFlow(CMUTIL_MemNode *node)
{
    if (node->flag != 0xFF) {
        if (node->stack) {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory underflow written detected while "
                        "freeing. which allocated from%s"S_CRLF
                        " freeing location is",
                        CMUTIL_CALL(node->stack, GetCString));
        } else {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory underflow written detected while "
                        "freeing.");
        }
        return CMUTIL_False;
    }
    if (*(((unsigned char*)node+sizeof(CMUTIL_MemNode)+node->size)) != 0xFF) {
        if (node->stack) {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory overflow written detected while "
                        "freeing. which allocated from%s"S_CRLF
                        " freeing location is",
                        CMUTIL_CALL(node->stack, GetCString));
        } else {
            CMUTIL_MemLogWithStack(
                        "*** FATAL - memory overflow written detected while "
                        "freeing.");
        }
        return CMUTIL_False;
    }
    return CMUTIL_True;
}

CMUTIL_STATIC void CMUTIL_MemRcyFree(void *ptr)
{
    CMUTIL_MemNode *node =
            (CMUTIL_MemNode*)(((char*)ptr) - sizeof(CMUTIL_MemNode));
    CMUTIL_MemRcyList *list = &g_cmutil_memrcyblocks[node->index];

    if (CMUTIL_CALL(list->used, Remove, node) == NULL) {
        CMUTIL_MemLogWithStack(
                    "*** FATAL - not allocated memory to be freed.");
        return;
    }

    if (!CMUTIL_MemCheckFlow(node))
        return;

    // clear stack
    if (node->stack) {
        CMUTIL_CALL(node->stack, Destroy);
        node->stack = NULL;
    }

    CMUTIL_CALL(list->mutex, Lock);

    node->next = list->head;
    list->head = node;
    list->avlcnt++;

    CMUTIL_CALL(list->mutex, Unlock);
}

CMUTIL_STATIC void *CMUTIL_MemRcyRealloc(void *ptr, size_t size)
{
    CMUTIL_MemNode *node =
            (CMUTIL_MemNode*)(((char*)ptr) - sizeof(CMUTIL_MemNode));
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
                               "requested memory too big("PRINT64U
                               ") to allocate.", (uint64)size);
        return NULL;
    }
    if (node->index == nidx) {
        CMUTIL_MemRcyList *list;
        list = &g_cmutil_memrcyblocks[nidx];
        CMUTIL_CALL(list->mutex, Unlock);
        list->usedsize += (size - node->size);
        node->size = size;
        CMUTIL_CALL(list->mutex, Unlock);
        // update stack information
        if (node->stack)
            CMUTIL_CALL(node->stack, Destroy);
        if (g_cmutil_memoper == CMUTIL_MemDebug) {
            node->stack = CMUTIL_StringCreateInternal(
                        &g_cmutil_memdebug_system, 256, NULL);
            CMUTIL_CALL(g_cmutil_memstackwalker, PrintStack, node->stack, 0);
        } else
            node->stack = NULL;
        *(((char*)ptr) + size) = 0xFF;
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

static CMUTIL_Mem_st g_cmutil_memdebug = {
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
                    CMUTIL_MemRcyComparator, CMUTIL_MemNodeDestroy);
    }
    g_cmutil_memstackwalker = CMUTIL_StackWalkerCreateInternal(
                &g_cmutil_memdebug_system);
    g_cmutil_memlog_mutex = CMUTIL_MutexCreateInternal(
                &g_cmutil_memdebug_system);
}

void CMUTIL_MemDebugInit(CMUTIL_MemOper memoper)
{
    g_cmutil_memoper = memoper;
    switch (memoper) {
    case CMUTIL_MemSystem:
        // do nothing
        break;
    default:
        CMUTIL_MemRcyInit();
    }
}

void CMUTIL_MemDebugClear()
{
    if (g_cmutil_memoper != CMUTIL_MemSystem) {
        int i;
        for (i=0; i<MEM_BLOCK_SZ; i++) {
            CMUTIL_MemRcyList *list = &g_cmutil_memrcyblocks[i];
            // TODO: print used items;
            if (CMUTIL_CALL(list->used, GetSize) > 0) {
                CMUTIL_MemLog("*** FATAL - index:%d count:%d total:%ld byte "
                        "memory leak detected.",
                        i, list->cnt - list->avlcnt, list->usedsize);
                if (g_cmutil_memoper == CMUTIL_MemDebug) {
                    int j;
                    for (j=0; j<CMUTIL_CALL(list->used, GetSize); j++) {
                        CMUTIL_MemNode *node = (CMUTIL_MemNode*)CMUTIL_CALL(
                                    list->used, GetAt, j);
                        CMUTIL_MemLog("* %dth memory leak. size:%d%s"S_CRLF,
                                      j, node->size,
                                      CMUTIL_CALL(node->stack, GetCString));
                    }
                }
            }
            CMUTIL_CALL(list->used, Destroy);
            while (list->head) {
                CMUTIL_MemNode *curr = list->head;
                list->head = curr->next;
                CMUTIL_MemNodeDestroy(curr);
            }
            CMUTIL_CALL(list->mutex, Destroy);
        }
        if (g_cmutil_memstackwalker) {
            CMUTIL_CALL(g_cmutil_memstackwalker, Destroy);
            g_cmutil_memstackwalker = NULL;
        }
        if (g_cmutil_memlog_mutex) {
            CMUTIL_CALL(g_cmutil_memlog_mutex, Destroy);
            g_cmutil_memlog_mutex = NULL;
        }
        __CMUTIL_Mem = &g_cmutil_memdebug_system;
        g_cmutil_memoper = CMUTIL_MemSystem;
    }
}
