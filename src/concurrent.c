
#include "functions.h"

#include <errno.h>

#if defined(MSWIN)
# include <process.h>
// usleep implementation for windows
# define usleep(x)	Sleep(x/1000)
#else
# include <sys/time.h>
# include <unistd.h>
#endif

#if !defined(MSWIN)
# if defined(APPLE)
#  include <dispatch/dispatch.h>
# else
#  include <semaphore.h>
# endif
# if defined(APPLE) || __STDC_VERSION__ < 201112L || defined(__STDC_NO_THREADS__)
#  include <pthread.h>
#  define MUTEX_T			pthread_mutex_t
#  define MUTEX_LOCK		pthread_mutex_lock
#  define MUTEX_TRYLOCK		pthread_mutex_trylock
#  define MUTEX_UNLOCK		pthread_mutex_unlock
#  define MUTEX_DESTROY		pthread_mutex_destroy
#  define COND_T			pthread_cond_t
#  define COND_WAIT			pthread_cond_wait
#  define COND_TIMEDWAIT	pthread_cond_timedwait
#  define COND_SIGNAL		pthread_cond_signal
#  define COND_DESTROY		pthread_cond_destroy
#  define THREAD_T			pthread_t
# else
#  define USE_THREADS_H_	1
#  include <threads.h>
#  define MUTEX_T			mtx_t
#  define MUTEX_LOCK		mtx_lock
#  define MUTEX_TRYLOCK		mtx_trylock
#  define MUTEX_UNLOCK		mtx_unlock
#  define MUTEX_DESTROY		mtx_destroy
#  define COND_T			cnd_t
#  define COND_WAIT			cnd_wait
#  define COND_TIMEDWAIT	cnd_timedwait
#  define COND_SIGNAL		cnd_signal
#  define COND_DESTROY		cnd_destroy
#  define THREAD_T			thrd_t
# endif
#endif

//CMUTIL_LogDefine("cmutil.concurrent")

#if defined(_MSC_VER)
const __int64 DELTA_EPOCH_IN_MICROSECS = 11644473600000000;

/* IN UNIX the use of the timezone struct is obsolete;
I don't know why you use it.
See http://linux.about.com/od/commands/l/blcmdl2_gettime.htm
But if you want to use this structure to know about GMT(UTC) diffrence from
your local time it will be next: tz_minuteswest is the real diffrence in minutes from
GMT(UTC) and a tz_dsttime is a flag indicates whether daylight is now in use
*/

int gettimeofday(struct timeval *tv/*in*/, struct timezone *tz/*in*/)
{
    FILETIME ft;
    __int64 tmpres = 0;
    TIME_ZONE_INFORMATION tz_winapi;
    int rez = 0;

    ZeroMemory(&ft, sizeof(ft));
    ZeroMemory(&tz_winapi, sizeof(tz_winapi));

    GetSystemTimeAsFileTime(&ft);

    tmpres = ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    /*converting file time to unix epoch*/
    tmpres /= 10;  /*convert into microseconds*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (__int32)(tmpres*0.000001);
    tv->tv_usec = (tmpres % 1000000);

    CMUTIL_UNUSED(tz);
    //_tzset(),don't work properly, so we use GetTimeZoneInformation
    //rez = GetTimeZoneInformation(&tz_winapi);
    //tz->tz_dsttime = (rez == 2) ? true : false;
    //tz->tz_minuteswest = tz_winapi.Bias + ((rez == 2) ? tz_winapi.DaylightBias : 0);

    return 0;
}
#endif

//*****************************************************************************
// CMUTIL_Cond implements
//*****************************************************************************

typedef struct CMUTIL_Cond_Internal {
    CMUTIL_Cond			base;		// condition API interface
#if defined(MSWIN)
    HANDLE				cond;		// event handle for windows
#else
    int					state;		// condition state
    CMUTIL_Bool			manualrst;	// manual-reset indicator
    MUTEX_T				mutex;		// condition mutex
    COND_T				cond;		// condition variable
#endif
    CMUTIL_Mem_st		*memst;
} CMUTIL_Cond_Internal;

/// \brief Wait for this condition is set.
///
/// This operation is blocked until this condition is set.
/// \param cond this condition object.
CMUTIL_STATIC void CMUTIL_CondWait(CMUTIL_Cond *cond)
{
    CMUTIL_Cond_Internal *icond = (CMUTIL_Cond_Internal*)cond;
#if defined(MSWIN)
    (void)WaitForSingleObject(icond->cond, INFINITE);
#else
    MUTEX_LOCK(&(icond->mutex));
    while (!icond->state)
        COND_WAIT(&(icond->cond), &(icond->mutex));
    if (icond->manualrst)
        COND_SIGNAL(&(icond->cond));
    else
        icond->state = 0;
    MUTEX_UNLOCK(&(icond->mutex));
#endif
}

#define ESDBC_ONE_SEC	(1000*1000*1000)

/// \brief Waits with timeout.
///
/// Waits until this condition is set or the time-out interval elapses.
/// \param cond this condition object.
/// \param millisec
///		The time-out interval, in milliseconds. If a nonzero value is
///		specified, this method waits until the condition is set or the
///		interval elapses. If millisec is zero, the function does not
///		enter a wait state if this condition is not set; it always
///		returns immediately.
/// \return CMUTIL_True if this condition is set in given interval,
///		CMUTIL_False otherwise.
CMUTIL_STATIC CMUTIL_Bool CMUTIL_CondTimedWait(
        CMUTIL_Cond *cond, long millisec)
{
    CMUTIL_Bool res = CMUTIL_False;
    CMUTIL_Cond_Internal *icond = (CMUTIL_Cond_Internal*)cond;
#if defined(MSWIN)
    DWORD dwRes = WaitForSingleObject(icond->cond, (DWORD)millisec);
    if (dwRes == WAIT_OBJECT_0)
        res = CMUTIL_True;
#else
    MUTEX_LOCK(&(icond->mutex));
    if (millisec == 0) {
        res = icond->state? CMUTIL_True:CMUTIL_False;
    } else {
        struct timeval now;
        struct timespec timeout;
        if (gettimeofday(&now, NULL) == 0) {
            int ir = 0;
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = (now.tv_usec + millisec * 1000) * 1000;
            timeout.tv_sec += timeout.tv_nsec / ESDBC_ONE_SEC;
            timeout.tv_nsec %= ESDBC_ONE_SEC;

            while (!icond->state && ir != ETIMEDOUT)
                ir = COND_TIMEDWAIT(
                        &(icond->cond), &(icond->mutex), &timeout);
            if (icond->state)
                res = CMUTIL_True;
        }
    }
    if (res) {
        if (icond->manualrst)
            COND_SIGNAL(&(icond->cond));
        else
            icond->state = 0;
    }
    MUTEX_UNLOCK(&(icond->mutex));
#endif

    return res;
}

/// \brief Sets this conidtion object.
///
/// The state of a manual-reset condition object remains set until it is
/// explicitly to the nonsignaled state by the Reset method. Any number
/// of waiting threads, or threads that subsequently begin wait operations
/// for this condition object by calling one of wait functions, can be
/// released while this condition state is signaled.
///
/// The state of an auto-reset condition object remains signaled until a
/// single waiting thread is released, at which time the system
/// automatically resets this condition state to nonsignaled. If no
/// threads are waiting, this condition object's state remains signaled.
///
/// If this condition object set already, calling this method will has no
/// effect.
/// \param cond this condition object.
CMUTIL_STATIC void CMUTIL_CondSet(CMUTIL_Cond *cond)
{
    CMUTIL_Cond_Internal *icond = (CMUTIL_Cond_Internal*)cond;
#if defined(MSWIN)
    (void)SetEvent(icond->cond);
#else
    icond->state = 1;
    COND_SIGNAL(&(icond->cond));
#endif
}

/// \brief Unsets this condition object.
///
/// The state of this condition object remains nonsignaled until it is
/// explicitly set to signaled by the Set method. This nonsignaled state
/// blocks the execution of any threads that have specified this condition
/// object in a call to one of wait methods.
///
/// This Reset method is used primarily for manual-reset condition object,
/// which must be set explicitly to the nonsignaled state. Auto-reset
/// condition objects automatically change from signaled to nonsignaled
/// after a single waiting thread is released.
/// \param cond this condition object.
CMUTIL_STATIC void CMUTIL_CondReset(CMUTIL_Cond *cond)
{
    CMUTIL_Cond_Internal *icond = (CMUTIL_Cond_Internal*)cond;
#if defined(MSWIN)
    (void)ResetEvent(icond->cond);
#else
    icond->state = 0;
#endif
}

/// \brief Destroys all resources related with this object.
/// \param cond this condition object.
CMUTIL_STATIC void CMUTIL_CondDestroy(CMUTIL_Cond *cond)
{
    if (cond) {
        CMUTIL_Cond_Internal *icond = (CMUTIL_Cond_Internal*)cond;
#if defined(MSWIN)
        CloseHandle(icond->cond);
#else
        MUTEX_DESTROY(&(icond->mutex));
        COND_DESTROY(&(icond->cond));
#endif
        icond->memst->Free(icond);
    }
}

static CMUTIL_Cond g_cmutil_cond = {
    CMUTIL_CondWait,
    CMUTIL_CondTimedWait,
    CMUTIL_CondSet,
    CMUTIL_CondReset,
    CMUTIL_CondDestroy
};

CMUTIL_Cond *CMUTIL_CondCreateInternal(
        CMUTIL_Mem_st *memst, CMUTIL_Bool manual_reset)
{
    CMUTIL_Cond_Internal *res = memst->Alloc(sizeof(CMUTIL_Cond_Internal));
#if !defined(MSWIN) && !defined(USE_THREADS_H_)
    pthread_mutexattr_t mta;
#endif
    memset(res, 0x0, sizeof(CMUTIL_Cond_Internal));

    memcpy(res, &g_cmutil_cond, sizeof(CMUTIL_Cond));
    res->memst = memst;

#if defined(MSWIN)
    res->cond = CreateEvent(NULL, manual_reset? TRUE:FALSE, FALSE, NULL);
#else
# if defined(USE_THREADS_H_)
    mtx_init(&(res->mutex), mtx_recursive);
    cnd_init(&(res->cond));
# else
    pthread_mutexattr_init(&mta);
    pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(res->mutex), &mta);
    pthread_cond_init(&(res->cond), NULL);
# endif
    res->state = 0;
    res->manualrst = manual_reset;
#endif

    return (CMUTIL_Cond*)res;
}

/// \brief Creates a condition object.
///
/// Creates a manual or auto resetting condition object.
///
/// \param manual_reset
///			If this parameter is CMUTIL_True, the function creates a
///			manual-reset condition object, which requires the use of the Reset
///			method to set the event state to nonsignaled. If this parameter is
///			CMUTIL_False, the function creates an auto-reset condition
///			object, and system automatically resets the event state to
///			nonsignaled after a single waiting thread has been released.
CMUTIL_Cond *CMUTIL_CondCreate(CMUTIL_Bool manual_reset)
{
    return CMUTIL_CondCreateInternal(__CMUTIL_Mem, manual_reset);
}


//*****************************************************************************
// CMUTIL_Mutex implements
//*****************************************************************************

typedef struct CMUTIL_Mutex_Internal {
    CMUTIL_Mutex		base;
#if defined(MSWIN)
    CRITICAL_SECTION	mutex;
#else
    MUTEX_T				mutex;
#endif
    CMUTIL_Mem_st		*memst;
} CMUTIL_Mutex_Internal;

/// \brief Lock this mutex object.
///
/// This mutex object shall be locked. If this mutex is already locked by
/// another thread, the calling thread shall block until this mutex becomes
/// available. This method shall return with this mutex object in the
/// locked state with the calling thread as its owner.
///
/// This mutex is recursive lockable object. This mutex shall maintain the
/// concept of lock count. When a thread successfully acquires a mutex for
/// the first time, the lock count shall be set to one. Every time a thread
/// relocks this mutex, the lock count shall be incremented by one. Each
/// time the thread unlocks the mutex, the lock count shall be decremented
/// by one. When the lock count reaches zero, the mutex shall become
/// available for other threads to acquire.
/// \param mutex This mutex object.
CMUTIL_STATIC void CMUTIL_MutexLock(CMUTIL_Mutex *mutex)
{
    CMUTIL_Mutex_Internal *imutex =
            (CMUTIL_Mutex_Internal *)mutex;
#if defined(MSWIN)
    EnterCriticalSection(&(imutex->mutex));
#else
    MUTEX_LOCK(&(imutex->mutex));
#endif
}

/// \brief Unlock this mutex object.
///
/// This mutex object shall be unlocked if lock count reaches zero.
/// \param mutex This mutex object.
CMUTIL_STATIC void CMUTIL_MutexUnlock(CMUTIL_Mutex *mutex)
{
    CMUTIL_Mutex_Internal *imutex =
            (CMUTIL_Mutex_Internal *)mutex;
#if defined(MSWIN)
    LeaveCriticalSection(&(imutex->mutex));
#else
    MUTEX_UNLOCK(&(imutex->mutex));
#endif
}

/// \brief Try to lock given mutex object.
/// \param	mutex	a mutex object to be tested.
/// \return	CMUTIL_True if mutex locked successfully,
///			CMUTIL_False if lock failed.
CMUTIL_STATIC CMUTIL_Bool CMUTIL_MutexTryLock(CMUTIL_Mutex *mutex)
{
    CMUTIL_Mutex_Internal *imutex =
            (CMUTIL_Mutex_Internal *)mutex;
#if defined(MSWIN)
    return TryEnterCriticalSection(&(imutex->mutex))?
            CMUTIL_True:CMUTIL_False;
#else
    return MUTEX_TRYLOCK(&(imutex->mutex)) == 0?
            CMUTIL_True:CMUTIL_False;
#endif
}

/// \brief Destroys all resources related with this object.
/// \param mutex This mutex object.
CMUTIL_STATIC void CMUTIL_MutexDestroy(CMUTIL_Mutex *mutex)
{
    CMUTIL_Mutex_Internal *imutex =
            (CMUTIL_Mutex_Internal *)mutex;
#if defined(MSWIN)
    DeleteCriticalSection(&(imutex->mutex));
#else
    MUTEX_DESTROY(&(imutex->mutex));
#endif
    imutex->memst->Free(imutex);
}

CMUTIL_Mutex __g_cmutil_mutex = {
        CMUTIL_MutexLock,
        CMUTIL_MutexUnlock,
        CMUTIL_MutexTryLock,
        CMUTIL_MutexDestroy
};

CMUTIL_Mutex *CMUTIL_MutexCreateInternal(CMUTIL_Mem_st *memst)
{
    CMUTIL_Mutex_Internal *res =
            memst->Alloc(sizeof(CMUTIL_Mutex_Internal));
#if !defined(MSWIN) && !defined(USE_THREADS_H_)
    pthread_mutexattr_t mta;
#endif

    memset(res, 0x0, sizeof(CMUTIL_Mutex_Internal));
    memcpy(res, &__g_cmutil_mutex, sizeof(CMUTIL_Mutex));

    res->memst = memst;

#if defined(MSWIN)
    InitializeCriticalSection(&(res->mutex));
#else
# if defined(USE_THREADS_H_)
    MUTEX_INIT(&(res->mutex), mtx_recursive);
# else
    /* setting up recursive mutex */
    pthread_mutexattr_init(&mta);
    pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(res->mutex), &mta);
# endif
#endif

    return (CMUTIL_Mutex*)res;
}

/// \brief Creates a mutex object.
///
/// Any thread of the calling process can use created mutex-object in a call
/// to lock methods. When a lock method returns, the waiting thread is
/// released to continue its execution.
/// This function creates a 'recursive lockable' mutex object.
CMUTIL_Mutex *CMUTIL_MutexCreate()
{
    return CMUTIL_MutexCreateInternal(__CMUTIL_Mem);
}




//*****************************************************************************
// CMUTIL_Thread implements
//*****************************************************************************

typedef struct CMUTIL_Thread_Internal {
    CMUTIL_Thread		base;
    void				*udata;
    void				*retval;
    void				*(*proc)(void*);
    CMUTIL_Bool			isrunning;
#if defined(MSWIN)
    HANDLE				thread;
#else
    THREAD_T			thread;
#endif
    unsigned int		id;
    char				*name;
    uint64				sysid;
    CMUTIL_Mem_st		*memst;
} CMUTIL_Thread_Internal;

typedef struct CMUTIL_Thread_Global_Context {
    CMUTIL_Mutex	*mutex;
    CMUTIL_Array	*threads;
    unsigned int	threadIndex;
} CMUTIL_Thread_Global_Context;

static CMUTIL_Thread_Global_Context *g_cmutil_thread_context = NULL;
static uint64 g_main_thread_sysid;

CMUTIL_STATIC int CMUTIL_ThreadComparator(const void *a, const void *b)
{
    CMUTIL_Thread_Internal *ia = (CMUTIL_Thread_Internal*)a;
    CMUTIL_Thread_Internal *ib = (CMUTIL_Thread_Internal*)b;
    if (ia->sysid > ib->sysid) return 1;
    else if (ia->sysid < ib->sysid) return -1;
    else return 0;
}

void CMUTIL_ThreadInit()
{
    g_main_thread_sysid = CMUTIL_ThreadSystemSelfId();
    g_cmutil_thread_context =
            __CMUTIL_Mem->Alloc(sizeof(CMUTIL_Thread_Global_Context));
    memset(g_cmutil_thread_context, 0x0, sizeof(CMUTIL_Thread_Global_Context));
    g_cmutil_thread_context->mutex = CMUTIL_MutexCreateInternal(__CMUTIL_Mem);
    g_cmutil_thread_context->threads = CMUTIL_ArrayCreateInternal(
                __CMUTIL_Mem, 10, CMUTIL_ThreadComparator, NULL);
}

void CMUTIL_ThreadClear()
{
    if (g_cmutil_thread_context->threads)
        CMUTIL_CALL(g_cmutil_thread_context->threads, Destroy);
    if (g_cmutil_thread_context->mutex)
        CMUTIL_CALL(g_cmutil_thread_context->mutex, Destroy);
    CMFree(g_cmutil_thread_context);
    g_cmutil_thread_context = NULL;
}

#if defined(MSWIN)
CMUTIL_STATIC DWORD WINAPI CMUTIL_ThreadProc(void *param)
#else
# if defined(USE_THREADS_H_)
CMUTIL_STATIC int CMUTIL_ThreadProc(void *param)
# else
CMUTIL_STATIC void *CMUTIL_ThreadProc(void *param)
# endif
#endif
{
    CMUTIL_Thread_Internal *iparam = (CMUTIL_Thread_Internal*)param;

    iparam->isrunning = CMUTIL_True;
    iparam->retval = iparam->proc(iparam->udata);

    CMUTIL_CALL(g_cmutil_thread_context->mutex, Lock);
    CMUTIL_CALL(g_cmutil_thread_context->threads, Remove, iparam);
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Unlock);
    iparam->isrunning = CMUTIL_False;

#if defined(MSWIN)
    _endthreadex(0);
    return 0;
#else
    return NULL;
#endif
}

CMUTIL_STATIC void *CMUTIL_ThreadJoin(CMUTIL_Thread *thread)
{
    CMUTIL_Thread_Internal *ithread = (CMUTIL_Thread_Internal*)thread;
    void *res = NULL;
    if (ithread->thread) {
#if defined(MSWIN)
        if (WaitForSingleObject(ithread->thread, INFINITE) == WAIT_OBJECT_0)
            res = ithread->retval;
        CloseHandle(ithread->thread);
#else
# if defined(USE_THREADS_H_)
        int ires = 0;
        if (thrd_join(ithread->thread, &ires) == thrd_success)
            res = ithread->retval;
# else
        if (pthread_join(ithread->thread, &res) == 0)
            res = ithread->retval;
# endif
#endif
    }
    if (ithread->name)
        ithread->memst->Free(ithread->name);
    ithread->memst->Free(ithread);
    return res;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ThreadIsRunning(CMUTIL_Thread *thread)
{
    CMUTIL_Thread_Internal *ithread = (CMUTIL_Thread_Internal*)thread;
    return ithread->isrunning;
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_ThreadStart(CMUTIL_Thread *thread)
{
    int ir = 0;
    CMUTIL_Thread_Internal *ithread = (CMUTIL_Thread_Internal*)thread;

#if defined(MSWIN)
    unsigned int tid;
    ithread->thread = (HANDLE)_beginthreadex(NULL, 0,
            (unsigned int(__stdcall*)(void*))CMUTIL_ThreadProc,
            ithread, 0, &tid);
    if (ithread->thread == INVALID_HANDLE_VALUE)
        ir = -1;
    ithread->sysid = (uint64)tid;
#else
# if defined(USE_THREADS_H_)
    ir = thrd_create(
        &(ithread->thread), CMUTIL_ThreadProc, ithread) == thrd_success? 0:1;
# else
    ir = pthread_create(&(ithread->thread), NULL, CMUTIL_ThreadProc, ithread);
# endif
    ithread->sysid = (uint64)ithread->thread;
#endif

    CMUTIL_CALL(g_cmutil_thread_context->mutex, Lock);
    CMUTIL_CALL(g_cmutil_thread_context->threads, Add, ithread);
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Unlock);

    return ir == 0? CMUTIL_True:CMUTIL_False;
}

CMUTIL_STATIC unsigned int CMUTIL_ThreadGetId(CMUTIL_Thread *thread)
{
    CMUTIL_Thread_Internal *ithread = (CMUTIL_Thread_Internal*)thread;
    return ithread->id;
}

CMUTIL_STATIC const char *CMUTIL_ThreadGetName(CMUTIL_Thread *thread)
{
    CMUTIL_Thread_Internal *ithread = (CMUTIL_Thread_Internal*)thread;
    return ithread->name;
}

static CMUTIL_Thread g_cmutil_thread = {
        CMUTIL_ThreadStart,
        CMUTIL_ThreadJoin,
        CMUTIL_ThreadIsRunning,
        CMUTIL_ThreadGetId,
        CMUTIL_ThreadGetName
};

CMUTIL_Thread *CMUTIL_ThreadCreateInternal(
        CMUTIL_Mem_st *memst, void*(*proc)(void*), void *udata,
        const char *name)
{
    CMUTIL_Thread_Internal *ithread =
            memst->Alloc(sizeof(CMUTIL_Thread_Internal));
    memset(ithread, 0x0, sizeof(CMUTIL_Thread_Internal));

    memcpy(ithread, &g_cmutil_thread, sizeof(CMUTIL_Thread));

    ithread->proc = proc;
    ithread->udata = udata;
    ithread->memst = memst;
    ithread->name = memst->Strdup(name);

    CMUTIL_CALL(g_cmutil_thread_context->mutex, Lock);
    ithread->id = ++(g_cmutil_thread_context->threadIndex);
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Unlock);

    return (CMUTIL_Thread*)ithread;
}

CMUTIL_Thread *CMUTIL_ThreadCreate(
        void*(*proc)(void*), void *udata, const char *name)
{
    return CMUTIL_ThreadCreateInternal(__CMUTIL_Mem, proc, udata, name);
}

unsigned int CMUTIL_ThreadSelfId()
{
    unsigned int res = 0;
    CMUTIL_Thread_Internal q, *r;
    q.sysid = CMUTIL_ThreadSystemSelfId();
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Lock);
    r = (CMUTIL_Thread_Internal*)CMUTIL_CALL(
        g_cmutil_thread_context->threads, Find, &q, NULL);
    if (r != NULL) res = r->id;
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Unlock);

    return res;
}

CMUTIL_Thread *CMUTIL_ThreadSelf()
{
    static CMUTIL_Thread_Internal thmain={
        {
            CMUTIL_ThreadStart,
            CMUTIL_ThreadJoin,
            CMUTIL_ThreadIsRunning,
            CMUTIL_ThreadGetId,
            CMUTIL_ThreadGetName
        },
        NULL,
        NULL,
        NULL,
        CMUTIL_True,
    #if defined(MSWIN)
        NULL,
    #else
        (THREAD_T)NULL,
    #endif
        0,
        "main",
        0,
        NULL
    };

    CMUTIL_Thread_Internal q, *r;
    if (thmain.memst == NULL) {
        thmain.sysid = g_main_thread_sysid;
        thmain.memst = __CMUTIL_Mem;
    }
    q.sysid = CMUTIL_ThreadSystemSelfId();
    if (q.sysid == thmain.sysid) return (CMUTIL_Thread*)&thmain;
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Lock);
    r = (CMUTIL_Thread_Internal*)CMUTIL_CALL(
        g_cmutil_thread_context->threads, Find, &q, NULL);
    CMUTIL_CALL(g_cmutil_thread_context->mutex, Unlock);
    return (CMUTIL_Thread*)r;
}

uint64 CMUTIL_ThreadSystemSelfId()
{
#if defined(MSWIN)
    return (uint64)GetCurrentThreadId();
#else
# if defined(USE_THREADS_H_)
    return (uint64)thrd_current();
# else
    return (uint64)pthread_self();
# endif
#endif
}



//*****************************************************************************
// CMUTIL_Semaphore implements
//*****************************************************************************

typedef struct CMUTIL_Semaphore_Internal {
    CMUTIL_Semaphore        base;
#if defined(MSWIN)
    HANDLE                  semp;
#elif defined(APPLE)
    dispatch_semaphore_t    semp;
#else
    sem_t                   semp;
#endif
    CMUTIL_Mem_st           *memst;
} CMUTIL_Semaphore_Internal;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_SemaphoreAcquire(
        CMUTIL_Semaphore *semaphore, long millisec)
{
    CMUTIL_Semaphore_Internal *isem = (CMUTIL_Semaphore_Internal*)semaphore;
#if defined(MSWIN)
    DWORD dwval = (DWORD)(millisec < 0? INFINITE:(unsigned long)millisec);
    dwval = WaitForSingleObject(isem->semp, dwval);
    return dwval == WAIT_OBJECT_0?
            CMUTIL_True:CMUTIL_False;
#elif defined(APPLE)
    dispatch_time_t dtime = dispatch_time(
                DISPATCH_TIME_NOW, NSEC_PER_MSEC * millisec);
    CMUTIL_Bool res = dispatch_semaphore_wait(isem->semp, dtime) == 0?
                CMUTIL_True:CMUTIL_False;
    return res;
#else
    if (millisec < 0) {
        return sem_wait(&(isem->semp)) == 0?
                CMUTIL_True:CMUTIL_False;
    } else {
        const long nonesec = 1000 * 1000 * 1000;
        struct timespec ts = {0,0};
        struct timeval tv = {0,0};
        ts.tv_sec = (int)(millisec / 1000);
        ts.tv_nsec = (millisec % 1000) * 1000 * 1000;
        gettimeofday(&tv, NULL);
        ts.tv_sec += tv.tv_sec;
        ts.tv_nsec += tv.tv_usec * 1000;
        if (ts.tv_nsec >= nonesec) {
            ts.tv_sec++;
            ts.tv_nsec -= nonesec;
        }
        return sem_timedwait(&(isem->semp), &ts) == 0?
                CMUTIL_True:CMUTIL_False;
    }
#endif
}

CMUTIL_STATIC void CMUTIL_SemaphoreRelease(CMUTIL_Semaphore *semaphore)
{
    CMUTIL_Semaphore_Internal *isem = (CMUTIL_Semaphore_Internal*)semaphore;
#if defined(MSWIN)
    ReleaseSemaphore(isem->semp, 1, NULL);
#elif defined(APPLE)
    dispatch_semaphore_signal(isem->semp);
#else
    sem_post(&(isem->semp));
#endif
}

CMUTIL_STATIC void CMUTIL_SemaphoreDestroy(CMUTIL_Semaphore *semaphore)
{
    CMUTIL_Semaphore_Internal *isem = (CMUTIL_Semaphore_Internal*)semaphore;
#if defined(MSWIN)
    CloseHandle(isem->semp);
#elif defined(APPLE)
    dispatch_release((dispatch_object_t)isem->semp);
#else
    sem_destroy(&(isem->semp));
#endif
    isem->memst->Free(isem);
}

static CMUTIL_Semaphore g_cmutil_semaphore = {
        CMUTIL_SemaphoreAcquire,
        CMUTIL_SemaphoreRelease,
        CMUTIL_SemaphoreDestroy
};

CMUTIL_Semaphore *CMUTIL_SemaphoreCreateInternal(
        CMUTIL_Mem_st *memst, int initcnt)
{
    int ir = 0;
    CMUTIL_Semaphore_Internal *isem =
            memst->Alloc(sizeof(CMUTIL_Semaphore_Internal));
    memset(isem, 0x0, sizeof(CMUTIL_Semaphore_Internal));

    memcpy(isem, &g_cmutil_semaphore, sizeof(CMUTIL_Semaphore));
    isem->memst = memst;

#if defined(MSWIN)
    isem->semp = CreateSemaphore(
            NULL, initcnt>1024? 1024:initcnt, 1024, NULL);
    if (isem->semp == NULL)
        ir = -1;
#elif defined(APPLE)
    isem->semp = dispatch_semaphore_create(initcnt);
#else
    ir = sem_init(&(isem->semp), 0, initcnt);
#endif

    if (ir != 0) {
        memst->Free(isem);
        isem = NULL;
    }

    return (CMUTIL_Semaphore*)isem;
}

CMUTIL_Semaphore *CMUTIL_SemaphoreCreate(int initcnt)
{
    return CMUTIL_SemaphoreCreateInternal(__CMUTIL_Mem, initcnt);
}



//*****************************************************************************
// CMUTIL_RWLock implements
//*****************************************************************************

// use internal read/write lock implementation to support recursive lock
#define RWLOCK_USE_INTERNAL		1

typedef struct CMUTIL_RWLock_Internal {
    CMUTIL_RWLock		base;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_Mutex		*rdcntmtx;
    CMUTIL_Mutex		*wrmtx;
    CMUTIL_Cond			*nordrs;
    int					rdcnt;
#else
#if defined(MSWIN)
    CRITICAL_SECTION	rdcntlock;
    CRITICAL_SECTION	wrlock;
    HANDLE				nordrs;
    int					rdcnt;
#else
    pthread_rwlock_t	rwlock;
#endif
#endif
    CMUTIL_Mem_st		*memst;
} CMUTIL_RWLock_Internal;

CMUTIL_STATIC void CMUTIL_RWLockReadLock(CMUTIL_RWLock *rwlock)
{
    CMUTIL_RWLock_Internal *irwlock = (CMUTIL_RWLock_Internal*)rwlock;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_CALL(irwlock->wrmtx, Lock);
    CMUTIL_CALL(irwlock->rdcntmtx, Lock);
    if (++(irwlock->rdcnt) == 1)
        CMUTIL_CALL(irwlock->nordrs, Reset);
    CMUTIL_CALL(irwlock->rdcntmtx, Unlock);
    CMUTIL_CALL(irwlock->wrmtx, Unlock);
#else
#if defined(MSWIN)
    /*
     * We need to lock the wrLock too, otherwise a writer could
     * do the whole of rwlock_wrlock after the rdCnt changed
     * from 0 to 1, but before the event was reset.
     */
    EnterCriticalSection(&(irwlock->wrlock));
    EnterCriticalSection(&(irwlock->rdcntlock));
    if (++(rwlock->rdcnt) == 1) {
        ResetEvent(irwlock->nordrs);
    }
    LeaveCriticalSection(&irwlock->rdcntlock);
    LeaveCriticalSection(&irwlock->wrlock);
#else
    pthread_rwlock_rdlock(&(irwlock->rwlock));
#endif
#endif
}

CMUTIL_STATIC void CMUTIL_RWLockReadUnlock(CMUTIL_RWLock *rwlock)
{
    CMUTIL_RWLock_Internal *irwlock = (CMUTIL_RWLock_Internal*)rwlock;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_CALL(irwlock->rdcntmtx, Lock);
    if ( (--irwlock->rdcnt) == 0)
        CMUTIL_CALL(irwlock->nordrs, Set);
    CMUTIL_CALL(irwlock->rdcntmtx, Unlock);
#else
#if defined(MSWIN)
    EnterCriticalSection(&(irwlock->rdcntlock));
    if (--irwlock->rdcnt == 0) {
        SetEvent(irwlock->nordrs);
    }
    LeaveCriticalSection(&irwlock->rdcntlock);
#else
    pthread_rwlock_unlock(&(irwlock->rwlock));
#endif
#endif
}

CMUTIL_STATIC void CMUTIL_RWLockWriteLock(CMUTIL_RWLock *rwlock)
{
    CMUTIL_RWLock_Internal *irwlock = (CMUTIL_RWLock_Internal*)rwlock;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_CALL(irwlock->wrmtx, Lock);
    CMUTIL_CALL(irwlock->nordrs, Wait);
#else
#if defined(MSWIN)
    EnterCriticalSection(&(irwlock->wrlock));
    WaitForSingleObject(irwlock->nordrs, INFINITE);

    /* wrLock remains locked.  */
#else
    pthread_rwlock_wrlock(&(irwlock->rwlock));
#endif
#endif
}

CMUTIL_STATIC void CMUTIL_RWLockWriteUnlock(CMUTIL_RWLock *rwlock)
{
    CMUTIL_RWLock_Internal *irwlock = (CMUTIL_RWLock_Internal*)rwlock;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_CALL(irwlock->wrmtx, Unlock);
#else
#if defined(MSWIN)
    LeaveCriticalSection(&irwlock->wrlock);
#else
    pthread_rwlock_unlock(&(irwlock->rwlock));
#endif
#endif
}

CMUTIL_STATIC void CMUTIL_RWLockDestroy(CMUTIL_RWLock *rwlock)
{
    CMUTIL_RWLock_Internal *irwlock = (CMUTIL_RWLock_Internal*)rwlock;
#if defined(RWLOCK_USE_INTERNAL)
    CMUTIL_CALL(irwlock->nordrs, Wait);
    CMUTIL_CALL(irwlock->nordrs, Destroy);
    CMUTIL_CALL(irwlock->rdcntmtx, Destroy);
    CMUTIL_CALL(irwlock->wrmtx, Destroy);
#else
#if defined(MSWIN)
    WaitForSingleObject(irwlock->nordrs,INFINITE);
    CloseHandle(irwlock->nordrs);
    DeleteCriticalSection(&(irwlock->rdcntlock));
    DeleteCriticalSection(&(irwlock->wrlock));
#else
    pthread_rwlock_destroy(&(irwlock->rwlock));
#endif
#endif
    irwlock->memst->Free(rwlock);
}

static CMUTIL_RWLock g_cmutil_rwlock = {
        CMUTIL_RWLockReadLock,
        CMUTIL_RWLockReadUnlock,
        CMUTIL_RWLockWriteLock,
        CMUTIL_RWLockWriteUnlock,
        CMUTIL_RWLockDestroy
};

CMUTIL_RWLock *CMUTIL_RWLockCreateInternal(CMUTIL_Mem_st *memst)
{
    CMUTIL_RWLock_Internal *irwlock =
            memst->Alloc(sizeof(CMUTIL_RWLock_Internal));
    memset(irwlock, 0x0, sizeof(CMUTIL_RWLock_Internal));

    memcpy(irwlock, &g_cmutil_rwlock, sizeof(CMUTIL_RWLock));

    irwlock->memst = memst;

#if defined(RWLOCK_USE_INTERNAL)
    irwlock->rdcntmtx = CMUTIL_MutexCreateInternal(memst);
    irwlock->wrmtx = CMUTIL_MutexCreateInternal(memst);
    irwlock->nordrs = CMUTIL_CondCreateInternal(memst, CMUTIL_True);
    CMUTIL_CALL(irwlock->nordrs, Set);
#else
#if defined(MSWIN)
    InitializeCriticalSection(&(irwlock->rdcntlock));
    InitializeCriticalSection(&(irwlock->wrlock));

    /*
     * We use a manual-reset event as poor man condition variable that
     * can only do broadcast.  Actually, only one thread will be waiting
     * on this at any time, because the wait is done while holding the
     * wrLock.
     */
    irwlock->noRdrs = CreateEvent (NULL, TRUE, TRUE, NULL);
#else
    pthread_rwlock_init(&(irwlock->rwlock), NULL);
#endif
#endif

    return (CMUTIL_RWLock*)irwlock;
}

CMUTIL_RWLock *CMUTIL_RWLockCreate()
{
    return CMUTIL_RWLockCreateInternal(__CMUTIL_Mem);
}


typedef enum CMUTIL_TimerTaskType {
    TimerTask_OneTime,
    TimerTask_Repeat
} CMUTIL_TimerTaskType;

typedef struct CMUTIL_TimerTask_Internal {
    CMUTIL_TimerTask		base;			// timer task API interface
    CMUTIL_TimerTaskType	type;			// timer task type
    CMUTIL_Bool				canceled;		// task canceled or not
    struct timeval			nextrun;		// next run time for repeating tasks
    long					period;			// repeating period
    void					(*proc)(void*);	// task procedure
    void					*param;			// procedure parameter
    CMUTIL_Timer			*timer;			// timer reference
} CMUTIL_TimerTask_Internal;

typedef struct CMUTIL_Timer_Internal {
    CMUTIL_Timer			base;		// timer API interface
    CMUTIL_Mutex			*mutex;		// timer task add/remove synchronization
    CMUTIL_Array			*scheduled;	// tasks in scheduled to run
    CMUTIL_List				*threads;	// worker thread pool
    CMUTIL_Array			*finished;	// finished tasks
    CMUTIL_Array			*alltasks;	// all tasks
    CMUTIL_List				*jqueue;	// queue of jobs to run
    CMUTIL_Semaphore		*jsemp;		// job queue semaphore
    CMUTIL_Thread			*mainloop;	// timer scheduling main loop thread
    CMUTIL_Bool				running;	// timer running indicator
    long					precision;	// timer precision
    int						numthrs;	// number of worker thread
    CMUTIL_Mem_st			*memst;
} CMUTIL_Timer_Internal;

CMUTIL_STATIC CMUTIL_Bool CMUTIL_TimerTaskCancelPrivate(
        CMUTIL_TimerTask *task)
{
    CMUTIL_TimerTask_Internal *itask = (CMUTIL_TimerTask_Internal*)task;
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)itask->timer;
    CMUTIL_Bool isfree = CMUTIL_False;

    CMUTIL_CALL(itimer->mutex, Lock);
    if (!itask->canceled) {
        if (CMUTIL_CALL(itimer->scheduled, Remove, task) ||
                CMUTIL_CALL(itimer->finished, Remove, task)) {
            CMUTIL_CALL(itimer->alltasks, Remove, task);
            isfree = CMUTIL_True;
        } else {
            itask->canceled = CMUTIL_True;
        }
    }
    CMUTIL_CALL(itimer->mutex, Unlock);

    if (isfree)
        itimer->memst->Free(itask);

    return isfree;
}

CMUTIL_STATIC void CMUTIL_TimerTaskCancel(CMUTIL_TimerTask *task)
{
    (void)CMUTIL_TimerTaskCancelPrivate(task);
}

CMUTIL_STATIC void CMUTIL_TimerGetDelayed(struct timeval *tv, long delay)
{
    tv->tv_sec += delay / 1000;
    tv->tv_usec += (delay % 1000) * 1000;
    if (tv->tv_usec >= 1000 * 1000) {
        tv->tv_sec++;
        tv->tv_usec -= 1000 * 1000;
    }
}

CMUTIL_STATIC CMUTIL_TimerTask *CMUTIL_TimerTaskCreate(
        CMUTIL_Timer *timer, CMUTIL_TimerTaskType type,
        struct timeval *first, long period, void *param,
        void (*proc)(void*))
{
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)timer;
    CMUTIL_TimerTask_Internal *res =
            itimer->memst->Alloc(sizeof(CMUTIL_TimerTask_Internal));
    memset(res, 0x0, sizeof(CMUTIL_TimerTask_Internal));

    res->timer = timer;
    memcpy(&(res->nextrun), first, sizeof(struct timeval));
    res->period = period;
    res->param = param;
    res->proc = proc;
    res->type = type;
    res->base.Cancel = CMUTIL_TimerTaskCancel;

    CMUTIL_CALL(itimer->mutex, Lock);
    CMUTIL_CALL(itimer->scheduled, Add, res);
    CMUTIL_CALL(itimer->alltasks, Add, res);
    CMUTIL_CALL(itimer->mutex, Unlock);

    return (CMUTIL_TimerTask*)res;
}

CMUTIL_STATIC CMUTIL_TimerTask *CMUTIL_TimerScheduleAtTime(
        CMUTIL_Timer *timer, struct timeval *attime,
        void (*proc)(void *param), void *param)
{
    return CMUTIL_TimerTaskCreate(
            timer, TimerTask_OneTime, attime, 0, param, proc);
}

CMUTIL_STATIC CMUTIL_TimerTask *CMUTIL_TimerScheduleDelay(
        CMUTIL_Timer *timer, long delay,
        void (*proc)(void *param), void *param)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    CMUTIL_TimerGetDelayed(&tv, delay);
    return CMUTIL_TimerTaskCreate(
            timer, TimerTask_OneTime, &tv, 0, param, proc);
}

CMUTIL_STATIC CMUTIL_TimerTask *CMUTIL_TimerScheduleAtRepeat(
        CMUTIL_Timer *timer, struct timeval *first, long period,
        void (*proc)(void *param), void *param)
{
    return CMUTIL_TimerTaskCreate(
            timer, TimerTask_Repeat, first, period, param, proc);
}

CMUTIL_STATIC CMUTIL_TimerTask *CMUTIL_TimerScheduleDelayRepeat(
        CMUTIL_Timer *timer, long delay, long period,
        void (*proc)(void *param), void *param)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    CMUTIL_TimerGetDelayed(&tv, delay);
    return CMUTIL_TimerTaskCreate(
            timer, TimerTask_Repeat, &tv, period, param, proc);
}

CMUTIL_STATIC void CMUTIL_TimerPurge(CMUTIL_Timer *timer)
{
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)timer;
    int i, len;

    CMUTIL_CALL(itimer->mutex, Lock);
    len = CMUTIL_CALL(itimer->alltasks, GetSize);
    for (i=0; i<len; i++) {
        CMUTIL_TimerTask_Internal *itask = (CMUTIL_TimerTask_Internal*)
                CMUTIL_CALL(itimer->alltasks, GetAt, i);
        if (CMUTIL_TimerTaskCancelPrivate((CMUTIL_TimerTask*)itask))
            len--, i--;
    }
    CMUTIL_CALL(itimer->mutex, Unlock);
}

CMUTIL_STATIC void CMUTIL_TimerDestroy(CMUTIL_Timer *timer)
{
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)timer;

    // cancel all tasks.
    CMUTIL_CALL(timer, Purge);
    itimer->running = CMUTIL_False;

    // join mainloop thread
    if (itimer->mainloop)
        CMUTIL_CALL(itimer->mainloop, Join);

    // destroy worker thread pool
    if (itimer->threads) {
        int i;
        for (i = 0; i < itimer->numthrs; i++)
            CMUTIL_CALL(itimer->jsemp, Release);
        for (i = 0; i < itimer->numthrs; i++) {
            CMUTIL_Thread *thr = (CMUTIL_Thread*)CMUTIL_CALL(
                    itimer->threads, RemoveFront);
            CMUTIL_CALL(thr, Join);
        }

        CMUTIL_CALL(itimer->threads, Destroy);
    }

    // remove scheduled tasks
    if (itimer->alltasks) {
        while (CMUTIL_CALL(itimer->alltasks, GetSize) > 0)
            itimer->memst->Free(CMUTIL_CALL(itimer->alltasks, RemoveAt, 0));
        CMUTIL_CALL(itimer->alltasks, Destroy);
    }

    if (itimer->jqueue)
        CMUTIL_CALL(itimer->jqueue, Destroy);

    if (itimer->finished)
        CMUTIL_CALL(itimer->finished, Destroy);

    if (itimer->scheduled)
        CMUTIL_CALL(itimer->scheduled, Destroy);

    if (itimer->jsemp)
        CMUTIL_CALL(itimer->jsemp, Destroy);

    if (itimer->mutex)
        CMUTIL_CALL(itimer->mutex, Destroy);

    itimer->memst->Free(itimer);
}

CMUTIL_STATIC int CMUTIL_TimerTVCompare(
        const struct timeval *a, const struct timeval *b)
{
    int sdiff = (int)(a->tv_sec - b->tv_sec);
    if (sdiff == 0) {
        return (int)(a->tv_usec - b->tv_usec);
    } else {
        return sdiff;
    }
}

CMUTIL_STATIC CMUTIL_Bool CMUTIL_TimerIsElapsed(
        void *task, struct timeval *curr)
{
    CMUTIL_TimerTask_Internal *itask = (CMUTIL_TimerTask_Internal*)task;
    if (CMUTIL_TimerTVCompare(&(itask->nextrun), curr) > 0)
        return CMUTIL_False;
    else
        return CMUTIL_True;
}

CMUTIL_STATIC void *CMUTIL_TimerMainLoop(void *param)
{
    struct timeval curr;
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)param;

    gettimeofday(&curr, NULL);

    while (itimer->running) {
        CMUTIL_TimerTask *task = NULL;

        CMUTIL_CALL(itimer->mutex, Lock);
        while (CMUTIL_TimerIsElapsed(
                CMUTIL_CALL(itimer->scheduled, Bottom), &curr)) {
            task = (CMUTIL_TimerTask*)
                    CMUTIL_CALL(itimer->scheduled, RemoveAt, 0);
            CMUTIL_CALL(itimer->jqueue, AddTail, task);
            CMUTIL_CALL(itimer->jsemp, Release);
        }
        CMUTIL_CALL(itimer->mutex, Unlock);

        usleep(itimer->precision);
        gettimeofday(&curr, NULL);
    }
    return NULL;
}

CMUTIL_STATIC void *CMUTIL_TimerWorker(void *param)
{
    CMUTIL_Timer_Internal *itimer = (CMUTIL_Timer_Internal*)param;
    while (itimer->running) {
        if (CMUTIL_CALL(itimer->jsemp, Acquire, 1000)) {
            CMUTIL_Array *addedto = NULL;
            CMUTIL_Bool isfree = CMUTIL_False;
            CMUTIL_TimerTask_Internal *itask;

            CMUTIL_CALL(itimer->mutex, Lock);
            itask = (CMUTIL_TimerTask_Internal*)CMUTIL_CALL(
                    itimer->jqueue, RemoveFront);
            CMUTIL_CALL(itimer->mutex, Unlock);

            if (itask == NULL) break;

            if (!itask->canceled) {
                itask->proc(itask->param);

                if (itask->type == TimerTask_Repeat) {
                    addedto = itimer->scheduled;

                    // calculate next run time.
                    CMUTIL_TimerGetDelayed(&(itask->nextrun), itask->period);
                }
                else {
                    addedto = itimer->finished;
                }
            }

            CMUTIL_CALL(itimer->mutex, Lock);
            if (itask->canceled) {
                CMUTIL_CALL(itimer->alltasks, Remove, itask);
                isfree = CMUTIL_True;
            }
            else if (addedto) {
                CMUTIL_CALL(addedto, Add, itask);
            }
            CMUTIL_CALL(itimer->mutex, Unlock);

            if (isfree)
                itimer->memst->Free(itask);
        }
    }
    return NULL;
}

CMUTIL_STATIC int CMUTIL_TimerTaskComp(const void *a, const void *b)
{
    const CMUTIL_TimerTask_Internal *ta = (CMUTIL_TimerTask_Internal*)a;
    const CMUTIL_TimerTask_Internal *tb = (CMUTIL_TimerTask_Internal*)b;
    return CMUTIL_TimerTVCompare(&(ta->nextrun), &(tb->nextrun));
}

CMUTIL_STATIC int CMUTIL_TimerAddrComp(const void *a, const void *b)
{
    return a < b? -1:a > b? 1:0;
}

static CMUTIL_Timer g_cmutil_timer = {
    CMUTIL_TimerScheduleAtTime,
    CMUTIL_TimerScheduleDelay,
    CMUTIL_TimerScheduleAtRepeat,
    CMUTIL_TimerScheduleDelayRepeat,
    CMUTIL_TimerPurge,
    CMUTIL_TimerDestroy
};

CMUTIL_Timer *CMUTIL_TimerCreateInternal(
        CMUTIL_Mem_st *memst, long precision, int threads)
{
    int i;
    CMUTIL_Timer_Internal *res = memst->Alloc(sizeof(CMUTIL_Timer_Internal));

    memset(res, 0x0, sizeof(CMUTIL_Timer_Internal));

    // Copy method callbacks
    memcpy(res, &g_cmutil_timer, sizeof(CMUTIL_Timer));

    res->memst = memst;
    res->running = CMUTIL_True;
    res->precision = precision;
    res->numthrs = threads;
    res->mutex = CMUTIL_MutexCreateInternal(memst);
    res->jsemp = CMUTIL_SemaphoreCreateInternal(memst, 0);
    res->jqueue = CMUTIL_ListCreateInternal(memst, NULL);

    // create timer task array which sorted by it's schedule time.
    res->scheduled = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_TimerTaskComp, NULL);

    // create
    res->finished = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_TimerAddrComp, NULL);
    res->alltasks = CMUTIL_ArrayCreateInternal(
                memst, 5, CMUTIL_TimerAddrComp, NULL);

    // create worker thread pool
    res->threads = CMUTIL_ListCreateInternal(memst, NULL);
    for (i = 0; i < threads; i++) {
        CMUTIL_Thread *thr = CMUTIL_ThreadCreateInternal(
                    memst, CMUTIL_TimerWorker, res, "TimerPool");
        CMUTIL_CALL(thr, Start);
        CMUTIL_CALL(res->threads, AddTail, thr);
    }

    // Create main loop thread.
    res->mainloop = CMUTIL_ThreadCreateInternal(
                memst, CMUTIL_TimerMainLoop, res, "TimerMain");

    return (CMUTIL_Timer*)res;
}

CMUTIL_Timer *CMUTIL_TimerCreateEx(long precision, int threads)
{
    return CMUTIL_TimerCreateInternal(__CMUTIL_Mem, precision, threads);
}

