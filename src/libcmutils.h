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

/*
 * File
 *      libcmutils.h
 *
 * Description
 *      libcmutils is a bunch of commonly used utility functions for
 *      multi-platform.
 *
 * Authors
 *      libcmutils: Dennis Soungjin Park <xcomart@gmail.com>,
 *      fpattern: David R. Tribble <david.tribble@beasys.com>
 */

#ifndef LIBCMUTILS_H__
#define LIBCMUTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__CYGWIN__)     /* CYGWIN implements a UNIX API */
# undef WIN
# undef _WIN
# undef _WIN32
# undef _WIN64
# undef MSWIN
#endif

#if (defined(_WIN32) || defined(_WIN64)) && !defined(MSWIN)
# define MSWIN
#endif

#if defined(__APPLE__) && !defined(APPLE)
# define APPLE
#endif

#if defined(_MSC_VER)
# include <windows.h>
#else
# include <sys/time.h>
# include <unistd.h>
# include <netinet/in.h>
#endif
#include <stdarg.h>
#include <stdint.h>

#define __STDC_FORMAT_MACROS 1 /* for 64bit integer-related macros */
#include <inttypes.h>
#include <sys/types.h>

#ifndef CMUTIL_API
# if defined(MSWIN)
#  if defined(CMUTIL_EXPORT)
#   define CMUTIL_API  __declspec(dllexport)
#  else
#   define CMUTIL_API  __declspec(dllimport)
#  endif
# else
#  define CMUTIL_API
# endif
#endif

#if !defined(MSWIN)
# if !defined(SOCKET)
#  define SOCKET            int
# endif
# if !defined(closesocket)
#  define closesocket       close
# endif
# if !defined(ioctlsocket)
#  define ioctlsocket       ioctl
# endif
# if !defined(SOCKET_ERROR)
#  define SOCKET_ERROR      -1
# endif
# if !defined(INVALID_SOCKET)
#  define INVALID_SOCKET    -1
# endif
#endif

/*
 * to remove a noisy warning in qtcreator
 */
#ifndef __null
# define __null (void*)0
#endif

/*
 * MSVC compatible definitions.
 */
#if defined(_MSC_VER)
# define strcasecmp     _stricmp
# define stat           _stat
# define pid_t          DWORD
# define usleep(x)      Sleep(x / 1000)
#endif


/**
 * @defgroup CMUTIL Types.
 * @{
 *
 * CMUTIL library uses platform-independent data types for multi-platform
 * support.
 */

/**
 * 64-bit signed / unsigned integer max values.
 */
#if !defined(INT64_MAX)
# define INT64_MAX  i64(0x7FFFFFFFFFFFFFFF)
#endif
#if !defined(UINT64_MAX)
# define UINT64_MAX ui64(0xFFFFFFFFFFFFFFFF)
#endif

/**
 * 64-bit signed / unsigned integer constant suffix.
 */
#define CMUTIL_APPEND(a,b)  a ## b
#if defined(_MSC_VER)
# define i64(x)     CMUTIL_APPEND(x, i64)
# define ui64(x)    CMUTIL_APPEND(x, ui64)
#else
# define i64(x)     CMUTIL_APPEND(x, LL)
# define ui64(x)    CMUTIL_APPEND(x, ULL)
#endif

#if defined(_MSC_VER)
# include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#if defined(_MSC_VER)
CMUTIL_API int __gettimeofday(struct timeval *tv, void *tz);

#define gettimeofday(tv, tz) __gettimeofday(tv, tz)
#endif


/**
 * @brief Boolean definition for this library.
 */
typedef enum CMBool {
    /** true */
    CMTrue         = 1,
    /** false */
    CMFalse        = 0
} CMBool;

/**
 * @}
 */

#if defined(APPLE)
/**
 * For Mac OS X.
 * Redefine gethostbyname for consistancy of gethostbyname_r implementation.
 */
# define gethostbyname_r    CMUTIL_NetworkGetHostByNameR
# if !defined(CMUTIL_EXPORT)
#  define gethostbyname    CMUTIL_NetworkGetHostByName
CMUTIL_API struct hostent *CMUTIL_NetworkGetHostByName(const char *name);
# endif
CMUTIL_API int CMUTIL_NetworkGetHostByNameR(
        const char *name, struct hostent *ret, char *buf, size_t buflen,
         struct hostent **result, int *h_errnop);
#endif

/**
 * Unused variable wrapper for avoiding compile warning.
 */
#define CMUTIL_UNUSED(a,...) CMUTIL_UnusedP((void*)(int64_t)(a), ## __VA_ARGS__)
CMUTIL_API void CMUTIL_UnusedP(void*,...);

/**
 * A wrapper macro for CMUTIL_CALL.
 */
#define CMUTIL_CALL__(a,b,...)  (a)->b((a), ## __VA_ARGS__)

/**
 * @brief Method caller for this library.
 *
 * This library is built on the C language, but some usage of this library
 * is similar to object-oriented languages like C++ or Java.
 *
 * Create an instance of the type CMUTIL_XX with CMUTIL_XXCreate function
 * and call the method with member callbacks.
 *
 * ie)<code>
 * // Create CMUTIL_XX object instance.
 * CMUTIL_XX *obj = CMUTIL_XXCreate();
 * // Call method FooBar with arguments(arg1, arg2).
 * obj->FooBar(obj, arg1, arg2);
 * // Destroy instance.
 * obj->Destroy(obj);
 * </code>
 *
 * As you see instance variable used redundantly in method call,
 * we created a macro CMCall because of this inconvenience.
 * As a result, the below code will produce the same result with the above code.
 * <code>
 * // Create CMUTIL_XX object instance.
 * CMUTIL_XX *obj = CMUTIL_XXCreate();
 * // Call method FooBar with arguments(arg1, arg2).
 * CMCall(obj, FooBar, arg1, arg2);
 * // Destroy instance.
 * CMCall(obj, Destroy);
 * </code>
 */
#define CMCall CMUTIL_CALL__
// for backward compatibility
#define CMUTIL_CALL CMUTIL_CALL__

/**
 * @brief Get a version string of this library.
 * @return Version string of this library.
 */
CMUTIL_API const char *CMUTIL_GetLibVersion(void);

/**
 * @typedef CMCompareCB Object comparison callback type.
 */
typedef int (*CMCompareCB)(const void*, const void*);

/**
 * @typedef CMFreeCB Callback type for memory deallocation.
 */
typedef void (*CMFreeCB)(void*);

/**
 * @typedef CMProcCB Callback type for execution of some procedure.
 */
typedef void (*CMProcCB)(void*);

/**
 * @defgroup CMUTILS_Initialization Initialization and memory operations.
 * @{
 *
 * libcmutils offers three types of memory management:
 *
 * <ul>
 *  <li> System version malloc, realloc, strdup, and free.
 *      This option has no memory management, proper for external memory
 *      debugger like 'duma' or 'valgrind'.</li>
 *  <li> Memory recycling, all memory allocation will be fit to 2^n sized memory
 *      blocks. This option prevents heap memory fragments,
 *      supports memory leak detection, and detects memory overflow/underflow
 *      corruption while freeing.</li>
 *  <li> Memory recycling with stack information.
 *      This option is the same as a previous recycling option,
 *      except all memory allocation will also create
 *      its callstack information, incredibly slow.
 *      Only use for memory leak debugging.</li>
 * </ul>
 */

/**
 * @typedef CMMemOper Memory operation types.
 * Refer CMUTIL_Init function for details.
 */
typedef enum CMMemOper {
    /** Use system version malloc, realloc, strdup, and free. */
    CMMemSystem = 0,
    /** Use memory recycle technique. */
    CMMemRecycle,
    /** Use memory operation with debugging information. */
    CMMemDebug
} CMMemOper;

/**
 * @brief Initialize this library. This function must be called before calling
 * any function in this library.
 *
 *  <ul>
 *   <li>CMMemSystem: All memory operations will be replaced with
 *         malloc, realloc, strdup, and free.
 *         Same as direct call system calls.</li>
 *   <li>CMMemRecycle: All memory allocation will be managed with pool.
 *         Pool is consists of memory blocks whose size is power of 2 bytes.
 *         Recommended for any purpose. This option prevents heap memory
 *         fragments and checks memory leak at termination. But a little
 *         overhead is required for recycling and needs much more memory
 *         usage.</li>
 *   <li>CMMemDebug: Option for memory debugging.
 *         Any memory operation will be traced, including stack tracing.
 *         Recommended only for memory debugging.
 *         Much slower than any other options.</li>
 *  </ul>
 *
 * @param memoper  Memory operation initializer. Must be one of
 *  CMUTIL_MemRelease, CMUTIL_MemDebug and CMUTIL_MemRecycle.
 *
 */
CMUTIL_API void CMUTIL_Init(CMMemOper memoper);

/**
 * @brief Clear allocated resource for this library.
 *
 * After clearing, all allocated resources are freed, and this library is
 * ready to be initialized again. If any resource is leaked,
 * it will be reported and CMFalse will be returned.
 *
 * @return CMTrue if all resources are cleared successfully or inited with CMMemSystem,
 *      CMFalse if some resources are leaked, when inited with CMMemRecycle or CMMemDebug.
 */
CMUTIL_API CMBool CMUTIL_Clear(void);

/**
 * Memory operation interface.
 */
typedef struct CMUTIL_Mem {
    /**
     * @brief Allocate memory.
     *
     * Allocates <code>size<code> bytes and returns a pointer to the allocated
     * memory. The memory is not initialized. If size is 0, then this function
     * returns either NULL or a unique pointer value that can later be
     * successfully passed to <code>Free</code>.
     *
     * @param size  Count of bytes to be allocated.
     * @return A pointer to the allocated memory, which is suitably aligned for
     *  any built-in type. On error this function returns NULL. NULL may also
     *  be returned by a successful call to this function with a
     *  <code>size</code> of zero.
     */
    void *(*Alloc)(
            size_t size);

    /**
     * @brief Allocate memory with zero filled.
     *
     * @param nmem  Count of memory elements to be allocated.
     * @param size  Size of each memory element.
     * @return A pointer to the allocated memory, which is suitably aligned for
     *  any built-in type. On error this function returns NULL. NULL may also
     *  be returned by a successful call to this function with a
     *  <code>size</code> of zero.
     */
    void *(*Calloc)(
            size_t nmem,
            size_t size);

    /**
     * @brief Reallocate memory.
     *
     * @param ptr   Pointer to the memory block to be reallocated.
     * @param size  New size of the memory block.
     * @return A pointer to the reallocated memory, which is suitably aligned for
     *  any built-in type. On error this function returns NULL. NULL may also
     *  be returned by a successful call to this function with a
     *  <code>size</code> of zero.
     */
    void *(*Realloc)(
            void *ptr,
            size_t size);

    /**
     * @brief Duplicate a string.
     *
     * @param str   Pointer to the string to be duplicated.
     * @return A pointer to the duplicated string, which is suitably aligned for
     *  any built-in type. On error this function returns NULL. NULL may also
     *  be returned by a successful call to this function with a
     *  <code>str</code> of NULL.
     */
    char *(*Strdup)(
            const char *str);

    /**
     * @brief Free memory.
     *
     * @param ptr   Pointer to the memory block to be freed.
     */
    void (*Free)(
            void *ptr);
} CMUTIL_Mem;

/**
 * @brief Global memory operator structure.
 * This object will be initialized with appropriate memory operators in
 * <tt>CMUTIL_Init</tt>.
 */
CMUTIL_API CMUTIL_Mem *CMUTIL_GetMem(void);

/**
 * @brief Allocate new memory.
 * Use this macro instead of malloc for debugging memory operations.
 */
#define CMAlloc     CMUTIL_GetMem()->Alloc

/**
 * @brief Allocate new memory with zero filled.
 * Use this macro instead of calloc.
 */
#define CMCalloc    CMUTIL_GetMem()->Calloc

/**
 * @brief Reallocate memory with the given size preserving previous data.
 */
#define CMRealloc   CMUTIL_GetMem()->Realloc

/**
 * @brief String duplication.
 */
#define CMStrdup    CMUTIL_GetMem()->Strdup

/**
 * @brief Deallocate memory which allocated with
 *  CMAlloc, CMCalloc, CMRealloc and CMStrdup.
 */
#define CMFree      CMUTIL_GetMem()->Free

/**
 * @}
 */

/**
 * @typedef CMUTIL_Cond Platform independent condition definition for
 * concurrency control.
 *
 * Condition (or Event)
 */
typedef struct CMUTIL_Cond CMUTIL_Cond;
struct CMUTIL_Cond {

    /**
     * @brief Wait for this condition is set.
     *
     * This operation is blocked until this condition is set.
     * @param cond this condition object.
     */
    void (*Wait)(
            CMUTIL_Cond *cond);

    /**
     * @brief Waits with timeout.
     *
     * Waits until this condition is set or the time-out interval elapses.
     * @param cond this condition object.
     * @param millisec
     *     The time-out interval, in milliseconds. If a nonzero value is
     *     specified, this method waits until the condition is set or the
     *     interval elapses. If millisec is zero, the function does not
     *     enter a wait state if this condition is not set, it always
     *     returns immediately.
     * @return CMTrue if this condition is set in given interval,
     *     CMFalse otherwise.
     */
    CMBool (*TimedWait)(
            CMUTIL_Cond *cond,
            long millisec);

    /**
     * @brief Sets this condition object.
     *
     * The state of a manual-reset condition object remains set until it is
     * explicitly to the nonsignaled state by the Reset method. Any number
     * of waiting threads, or threads that subsequently begin wait operations
     * for this condition object by calling one of the wait functions, can be
     * released while this condition state is signaled.
     *
     * The state of an auto-reset condition object remains signaled until a
     * single waiting thread is released, at which time the system
     * automatically resets this condition state to nonsignaled. If no
     * threads are waiting, this condition object's state remains signaled.
     *
     * If this condition object is set already, calling this method
     * will have no effect.
     * @param cond this condition object.
     */
    void (*Set)(
            CMUTIL_Cond *cond);

    /**
     * @brief Unsets this condition object.
     *
     * The state of this condition object remains nonsignaled until it is
     * explicitly set to signaled state by the Set method.
     * This nonsignaled state blocks the execution of any threads
     * that have specified this condition object in a call to
     * one of wait methods.
     *
     * This Reset method is used primarily for a manual-reset condition object,
     * which must be set explicitly to the nonsignaled state. Auto-reset
     * condition objects automatically change from signaled to nonsignaled
     * after a single waiting thread is released.
     * @param cond this condition object.
     */
    void (*Reset)(
            CMUTIL_Cond *cond);

    /**
     * @brief Destroys all resources related to this object.
     * @param cond this condition object.
     */
    void (*Destroy)(
            CMUTIL_Cond *cond);
};

/**
 * @brief Creates a condition object.
 *
 * Creates a manual or auto-resetting condition object.
 *
 * @param manual_reset
 *     If this parameter is CMTrue, the function creates a
 *     manual-reset condition object, which requires the use of the Reset
 *     method to set the event state to nonsignaled. If this parameter is
 *     CMFalse, the function creates an auto-reset condition
 *     object, and the system automatically resets the event state to
 *     nonsignaled after a single waiting thread has been released.
 * @return Created a conditional object.
 */
CMUTIL_API CMUTIL_Cond *CMUTIL_CondCreate(CMBool manual_reset);

/**
 * @typedef CMUTIL_Mutex Platform independent mutex implementation.
 */
typedef struct CMUTIL_Mutex CMUTIL_Mutex;
struct CMUTIL_Mutex {

    /**
     * @brief Lock this mutex object.
     *
     * This mutex object shall be locked. If this mutex is already locked by
     * another thread, the calling thread shall block until this mutex becomes
     * available. This method shall return with this mutex object in the
     * locked state with the calling thread as its owner.
     *
     * This mutex is a recursive lockable object. This mutex shall maintain the
     * concept of lock count. When a thread successfully acquires a mutex for
     * the first time, the lock count shall be set to one. Every time a thread
     * relocks this mutex, the lock count shall be incremented by one. Each
     * time the thread unlocks the mutex, the lock count shall be decremented
     * by one. When the lock count reaches zero, the mutex shall become
     * available for other threads to acquire.
     * @param mutex This mutex object.
     */
    void (*Lock)(CMUTIL_Mutex *mutex);

    /**
     * @brief Unlock this mutex object.
     *
     * This mutex object shall be unlocked if the lock count reaches zero.
     * @param mutex This mutex object.
     */
    void (*Unlock)(CMUTIL_Mutex *mutex);

    /**
     * @brief Try to lock the given mutex object.
     * @param  mutex   a mutex object to be tested.
     * @return CMTrue if mutex locked successfully,
     *         CMFalse if another thread locked this mutex already,
     *         so the locking is failed.
     */
    CMBool (*TryLock)(CMUTIL_Mutex *mutex);

    /**
     * @brief Destroys all resources related to this object.
     * @param mutex This mutex object.
     */
    void (*Destroy)(CMUTIL_Mutex *mutex);
};

/**
 * @brief Creates a mutex object.
 *
 * Any thread of the calling process can use created mutex-object in a call
 * to lock methods. When a lock method returns, the waiting thread is
 * released to continue its execution.
 * This function creates a 'recursive lockable' mutex object.
 * @return Created a mutex object.
 */
CMUTIL_API CMUTIL_Mutex *CMUTIL_MutexCreate(void);

/**
 * @brief Synchronized block macro.
 *
 * This macro will lock the given mutex object, execute given statements,
 * and unlock the given mutex object.
 *
 * Note that do not use 'return', 'break', 'continue' or "goto" statements
 * inside synchronized block, because these statements will bypass
 * unlocking mutex operation, which will cause deadlock.
 *
 * @param a Mutex object to be locked.
 * @param ... Statements to be executed in synchronized block.
 */
#define CMSync(a, ...) do {     \
    CMCall((a), Lock);          \
    __VA_ARGS__                 \
    CMCall((a), Unlock);        \
} while(0)

/**
 * @typedef CMUTIL_Thread Platform independent thread object.
 */
typedef struct CMUTIL_Thread CMUTIL_Thread;
struct CMUTIL_Thread {

    /**
     * @brief Starts this thread.
     *
     * This function creates a new thread which is not detached,
     * so the created thread must be joined by calling Join method.
     * @param thread This thread object.
     * @return This thread has been started successfully or not.
     */
    CMBool (*Start)(CMUTIL_Thread *thread);

    /**
     * @brief Join this thread.
     *
     * This function joins this thread and cleans up its resources, including
     * this thread object, so these references must not be used after
     * calling this method. Every thread that been created by this library
     * must be joined by calling this method.
     * @param thread This thread object.
     * @return Thread return value.
     */
    void *(*Join)(CMUTIL_Thread *thread);

    /**
     * @brief This thread is running or not.
     *
     * @param thread This thread object.
     * @return CMTrue if this thread is running,
     *         CMFalse if this thread is not running.
     */
    CMBool (*IsRunning)(const CMUTIL_Thread *thread);

    /**
     * @brief Get the ID fo this thread. Returned ID is not system thread ID.
     *     just an internal thread index.
     * @return ID of this thread.
     */
    uint32_t (*GetId)(const CMUTIL_Thread *thread);

    /**
     * @brief Get the name of this thread.
     * @return Name of this thread.
     */
    const char *(*GetName)(const CMUTIL_Thread *thread);
};

/**
 * @brief Creates a thread object.
 *
 * Created thread does not start automatically.
 * Call the <code>Start</code> method to start this thread.
 * The returned object must be freed by calling the <tt>Join</tt> method,
 * even if this thread is not started.
 *
 * @param proc The start routine of the created thread.
 * @param udata This argument is passed as the sole argument of 'proc'
 * @param name Thread name, any name could be assigned
 *      and can also duplicable. But must not exceed 200 bytes.
 * @return Created a thread object.
 */
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadCreate(
        void*(*proc)(void*), void *udata, const char *name);

/**
 * @brief Get the ID of the current thread. This id is not system thread id,
 *     just an internal thread index.
 * @return ID of the current thread.
 */
CMUTIL_API uint32_t CMUTIL_ThreadSelfId(void);

/**
 * @brief Get the current thread context.
 * @return Current thread context.
 */
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadSelf(void);

/**
 * @brief Get system dependent thread id.
 * @return System dependent thread id.
 */
CMUTIL_API uint64_t CMUTIL_ThreadSystemSelfId(void);

/**
 * @typedef CMUTIL_ThreadPool A threadpool object.
 */
typedef struct CMUTIL_ThreadPool CMUTIL_ThreadPool;
struct CMUTIL_ThreadPool {

    /**
     * @brief Execute a job in this threadpool.
     *
     * This method will return immediately. Job will be queued and executed
     * when any idle thread is available in this pool.
     *
     * @param tp This threadpool object.
     * @param runnable A callback function to be executed.
     * @param udata User data object which would be passed to
     *          the <code>runnable</code> callback function.
     */
    void (*Execute)(CMUTIL_ThreadPool *tp, CMProcCB runnable, void *udata);

    /**
     * @brief Wait until all jobs are finished.
     *
     * @param tp This threadpool object
     */
    void (*Wait)(CMUTIL_ThreadPool *tp);

    /**
     * @brief Destroy all resources related with this object
     * @param tp This threadpool object
     */
    void (*Destroy)(CMUTIL_ThreadPool *tp);
};

/**
 * @brief Creates a new threadpool object.
 *
 * This function will create a new threadpool object with the initial size.
 * If <code>pool_size</code> is zero or negative value, this threadpool object's
 * pool size will not be fixed, will be increased one by one if needed.
 *
 * @param pool_size Thread count of this threadpool if positive, otherwise
 *              this object will increase pool size automatically.
 * @param name Name of this threadpool object, any name could be assigned
 *              and can also duplicable. But must not exceed 200 bytes.
 * @return A new threadpool object.
 */
CMUTIL_API CMUTIL_ThreadPool *CMUTIL_ThreadPoolCreate(
        int pool_size, const char *name);

/**
 * @typedef CMUTIL_Semaphore Platform independent semaphore object.
 */
typedef struct CMUTIL_Semaphore CMUTIL_Semaphore;
struct CMUTIL_Semaphore {

    /**
     * @brief Acquire ownership from semaphore.
     *
     * Acquire ownership in the given time or fail with timed out. This method
     * will be blocked until ownership is acquired successfully in time,
     * or the waiting time expired.
     *
     * @param semaphore This semaphore object.
     * @param millisec Waiting time to acquire ownership, in millisecond.
     * @return CMTrue if ownership acquired successfully in time,
     *         CMFalse if failed to acquire ownership in time.
     */
    CMBool (*Acquire)(
            CMUTIL_Semaphore *semaphore, long millisec);

    /**
     * @brief Release ownership to semaphore.
     *
     * Release ownership to semaphore, the semaphore value increased
     * greater than zero consequently, one of the threads which is blocked
     * in the Acquire method will get the ownership and be unblocked.
     * @param semaphore This semaphore object.
     */
    void (*Release)(
            CMUTIL_Semaphore *semaphore);

    /**
     * @brief Destroy all resources related to this object.
     * @param semaphore This semaphore object.
     */
    void (*Destroy)(CMUTIL_Semaphore *semaphore);
};

/**
 * @brief Creates a semaphore object.
 *
 * Create an in-process semaphore object.
 * @param initcnt Initial semaphore ownership count.
 * @return Created a semaphore object.
 */
CMUTIL_API CMUTIL_Semaphore *CMUTIL_SemaphoreCreate(int initcnt);


/**
 * @typedef CMUTIL_RWLock Platform independent read/write lock object.
 */
typedef struct CMUTIL_RWLock CMUTIL_RWLock;
struct CMUTIL_RWLock {

    /**
     * @brief Read lock for this object.
     *
     * This method shall apply a read lock to the given object.
     * The calling thread acquires the read lock if a writer does not hold
     * the lock and there are no writers blocked on the lock.
     * @param rwlock This read-write lock object.
     */
    void (*ReadLock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Release read lock for this object.
     *
     * This method shall release a read lock held on the given object.
     * @param rwlock This read-write lock object.
     */
    void (*ReadUnlock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Write lock for this object.
     *
     * This method shall apply a write lock to the given object.
     * The calling thread acquires the write lock
     * if no other thread (reader or writer) holds the given object.
     * Otherwise, the thread shall block until it can acquire the lock.
     * The calling thread may deadlock if at the time
     * the call is made it holds the read-write lock
     * (whether a read or write lock).
     * @param rwlock This read-write lock object.
     */
    void (*WriteLock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Release write lock for this object.
     *
     * This method shall release a write lock held on the given object.
     * @param rwlock This read-write lock object.
     */
    void (*WriteUnlock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Destroy all resources related to this object.
     * @param rwlock This read-write lock object.
     */
    void (*Destroy)(CMUTIL_RWLock *rwlock);
};

/**
 * @brief Create a read-write lock object.
 */
CMUTIL_API CMUTIL_RWLock *CMUTIL_RWLockCreate(void);

/**
 * @typedef CMUTIL_Iterator Iterator of collection members.
 */
typedef struct CMUTIL_Iterator CMUTIL_Iterator;
struct CMUTIL_Iterator {

    /**
     * @brief Check iterator has the next element.
     * @param iter This iterator object.
     * @return CMTrue if this iterator has the next element.
     *         CMFalse if there are no more elements.
     */
    CMBool (*HasNext)(
            const CMUTIL_Iterator *iter);

    /**
     * @brief Get the next element from this iterator.
     * @param iter This iterator object.
     * @return Next element.
     */
    void *(*Next)(
            CMUTIL_Iterator *iter);

    /**
     * @brief Destroy this iterator.
     *
     * This method must be called after using this iterator.
     * @param iter This iterator object.
     */
    void (*Destroy)(
            CMUTIL_Iterator *iter);
};


/**
 * @typedef CMUTIL_Array Dynamic array of any type element.
 */
typedef struct CMUTIL_Array CMUTIL_Array;
struct CMUTIL_Array {

    /**
     * @brief Add a new element to this array.
     *
     * If this array is a sorted array (created including
     * <code>comparator</code> callback function), this method will add
     * a new item to the appropriate location.
     * Otherwise, this method will add a new item to the end of this array.
     * If the <code>freecb</code> is supplied at the creation, this array
     * will take the ownership of the item.
     *
     * @param array This dynamic array object.
     * @param item Item to be added.
     * @return Previous data at that position if this array is a sorted array.
     *         NULL if this is not a sorted array.
     */
    void *(*Add)(CMUTIL_Array *array, void *item);

    /**
     * @brief Remove an item from this array.
     *
     * This method will work both this array is sorted or not.
     * But if this array is sorted, more efficient binary search will be used,
     * otherwise linear search will be used.
     * The <code>compval</code> will be compared with array elements in
     * a binary search manner if this array is sorted.
     * If this array is not sorted, the memory address will be compared
     * to search the item.
     * The ownership of the item will be moved to caller.
     *
     * @param array This dynamic array object.
     * @param compval Search key of an item to be removed.
     * @return The removed element if given item found in this array.
     *         NULL if the given key does not exist in this array.
     */
    void *(*Remove)(CMUTIL_Array *array, const void *compval);

    /**
     * @brief Insert a new element to this array.
     *
     * This method will only work if this array is not a sorted array.
     * New item will be stored at the given <code>index</code>.
     * If the <code>freecb</code> is supplied at the creation, this array
     * will take the ownership of the item.
     *
     * @param array This dynamic array object.
     * @param item Item to be inserted.
     * @param index The index of this array where the item will be stored in.
     * @return Inserted item if this array is not a sorted array.
     *         NULL if this array is a sorted array or
     *         the index is out of bound.
     */
    void *(*InsertAt)(CMUTIL_Array *array, void *item, uint32_t index);

    /**
     * @brief Remove an item from this array.
     *
     * The item at the index of this array will be removed.
     * The ownership of the item will be moved to caller.
     *
     * @param array This dynamic array object.
     * @param index The index of this array which item will be removed.
     * @return The removed item. NULL if the index is out of bound.
     */
    void *(*RemoveAt)(CMUTIL_Array *array, uint32_t index);

    /**
     * @brief Replace an item at the position of this array.
     *
     * This method will only work if this array is not a sorted array.
     * The item positioned at the <code>index</code> of this array will be
     * replaced with the given <code>item</code>.
     * If the <code>freecb</code> is supplied at the creation, this array
     * will take the ownership of the item.
     *
     * @param array This dynamic array object.
     * @param item New item which will replace the old one.
     * @param index The index of this array, the item in which will be replaced.
     * @return Replaced old item if this array is not a sorted array.
     *         NULL if this array is a sorted array or
     *         the index is out of bound.
     */
    void *(*SetAt)(CMUTIL_Array *array, void *item, uint32_t index);

    /**
     * @brief Get an item from this array.
     *
     * Get an item which is stored at a given <code>index</code>.
     * The ownership of the item does not be moved to the caller.
     *
     * @param array This dynamic array object.
     * @param index The index of this array,
     *        the item in which will be retreived.
     * @return An item which positioned at <code>index</code> of this array.
     *         NULL if the given index is out of bound.
     */
    void *(*GetAt)(const CMUTIL_Array *array, uint32_t index);

    /**
     * @brief Find an item from this array.
     *
     * This method will use binary search if this is a sorted array.
     * Otherwise, linear search is used with memory address comparison.
     * The ownership of the item does not be moved to the caller.
     *
     * @param array This dynamic array object.
     * @param compval Search key of item which to be found.
     * @param index The index reference of the item which found with
     *        <code>compval</code>.
     *        Where the found item index will be stored in.
     * @return A found item from this array if the item found.
     *         NULL if the item is not found.
     */
    void *(*Find)(const CMUTIL_Array *array, const void *compval, uint32_t *index);

    /**
     * @brief The size of this array.
     *
     * @param array This dynamic array object.
     * @return The size of this array.
     */
    size_t  (*GetSize)(const CMUTIL_Array *array);

    /**
     * @brief Push an item to the end of this array like a stack operation.
     *
     * This method will only work if this array is not a sorted array.
     * This operation will add the given <code>item</code>
     * at the end of this array.
     * If the <code>freecb</code> is supplied at the creation, this array
     * will take the ownership of the item.
     *
     * @param array This dynamic array object.
     * @param item A new item to be pushed to this array.
     * @return CMTrue if push operation performed successfully.
     *         CMFalse otherwise.
     */
    CMBool (*Push)(CMUTIL_Array *array, void *item);

    /**
     * @brief Pop an item from the end of this array like a stack operation.
     *
     * This operation will remove an item at the end of this array.
     * The ownership of the item will be moved to the caller.
     *
     * @param array This dynamic array object.
     * @return Removed item if there are elements exists. NULL if there are
     *         no more elements.
     */
    void *(*Pop)(CMUTIL_Array *array);

    /**
     * @brief Get the top element from this array like a stack operation.
     *
     * Get the item at the end of this array.
     * The ownership of the item does not be moved to the caller.
     *
     * @param array This dynamic array object.
     * @return An item at the end of this array.
     *         NULL if there is no item in this array.
     */
    void *(*Top)(const CMUTIL_Array *array);

    /**
     * @brief Get the bottom element from this array like a stack operation.
     *
     * Get the item at the beginning of this array.
     * The ownership of the item does not be moved to the caller.
     *
     * @param array This dynamic array object.
     * @return An item at the beginning of this array.
     *         NULL if there is no item in this array.
     */
    void *(*Bottom)(const CMUTIL_Array *array);

    /**
     * @brief Get iterator of all items in this array.
     *
     * @param array This dynamic array object.
     * @return An {@link CMUTIL_Iterator CMUTIL_Iterator} object of
     *         all items in this array.
     * @see {@link CMUTIL_Iterator CMUTIL_Iterator}
     */
    CMUTIL_Iterator *(*Iterator)(const CMUTIL_Array *array);

    /**
     * @brief Clear this array object.
     *
     * Clear all items in this array object and make its size to zero.
     * If the <code>freecb</code> parameter is supplied at creation,
     * this callback will be called to all items in this array.
     *
     * @param array This dynamic array object.
     */
    void (*Clear)(CMUTIL_Array *array);

    /**
     * @brief Destroy this array object.
     *
     * Destroy this object and its internal allocations.
     * If the <code>freecb</code> parameter is supplied at creation,
     * this callback will be called to all items in this array.
     *
     * @param array This dynamic array object.
     */
    void (*Destroy)(CMUTIL_Array *array);
};

/**
 * Default initial capacity of a dynamic array.
 */
#define CMUTIL_ARRAY_DEFAULT    10

/**
 * @brief Creates an unsorted dynamic array.
 *
 * @return A new dynamic array.
 */
#define CMUTIL_ArrayCreate()    \
        CMUTIL_ArrayCreateEx(CMUTIL_ARRAY_DEFAULT, NULL, NULL)

/**
 * @brief Creates a dynamic array with capacity and optional comparator
 *  and free callback.
 *
 * @param initcapacity Initial capacity of this array.
 * @param comparator Comparator callback for a sorted array. You must provide
 *        this callback if you want to create a sorted array.
 * @param freecb Free callback for each item in this array, if this parameter
 *        is supplied, this array will take the ownership of its items.
 * @return A new dynamic array.
 */
CMUTIL_API CMUTIL_Array *CMUTIL_ArrayCreateEx(
        size_t initcapacity,
        CMCompareCB comparator,
        CMFreeCB freecb);

/**
 * @typedef CMUTIL_String Multifunctional string type.
 */
typedef struct CMUTIL_String CMUTIL_String;
struct CMUTIL_String {
    /**
     * @brief Add a c-style string to this string object.
     *
     * Append the given string to the end of this string object.
     *
     * @param string This string object.
     * @param tobeadded C-style null terminated string to be appended.
     * @return New size of this string object.
     */
    ssize_t (*AddString)(
            CMUTIL_String *string, const char *tobeadded);

    /**
     * @brief Add n bytes string to this string object.
     *
     * Append the given string to the end of this string object.
     * Given string is not needed to be null terminated.
     *
     * @param string This string object.
     * @param tobeadded C style string to be appended.
     * @param size Number of bytes to be appended from the given string.
     * @return New size of this string object.
     */
    ssize_t (*AddNString)(
            CMUTIL_String *string, const char *tobeadded, size_t size);

    /**
     * @brief Add a character to this string object.
     *
     * Append the given character to the end of this string object.
     *
     * @param string This string object.
     * @param tobeadded Character to be appended.
     * @return New size of this string object.
     */
    ssize_t (*AddChar)(
            CMUTIL_String *string, char tobeadded);

    /**
     * @brief Add a formatted string to this string object.
     *
     * Append a ormatted string to the end of this string object.
     *
     * @param string This string object.
     * @param fmt Format string like a printf function.
     * @param ... Arguments for format string.
     * @return New size of this string object.
     */
    ssize_t (*AddPrint)(
            CMUTIL_String *string, const char *fmt, ...);

    /**
     * @brief Add a formatted string to this string object.
     *
     * Append a formatted string to the end of this string object.
     *
     * @param string This string object.
     * @param fmt Format string like a vprintf function.
     * @param args Arguments list for format string.
     * @return New size of this string object.
     */
    ssize_t (*AddVPrint)(
            CMUTIL_String *string, const char *fmt, va_list args);

    /**
     * @brief Add another string object to this string object.
     *
     * Append the given string object to the end of this string object.
     *
     * @param string This string object.
     * @param tobeadded Another string object to be appended.
     * @return New size of this string object.
     */
    ssize_t (*AddAnother)(
            CMUTIL_String *string, const CMUTIL_String *tobeadded);

    /**
     * @brief Insert a c-style string to this string object.
     *
     * Insert the given string to this string object at the index.
     *
     * @param string This string object.
     * @param tobeadded C style null terminated string to be inserted.
     * @param at Index where the given string will be inserted.
     * @return New size of this string object.
     */
    ssize_t (*InsertString)(
            CMUTIL_String *string, const char *tobeadded, uint32_t at);

    /**
     * @brief Insert n bytes string to this string object.
     *
     * Insert a substring of the given string to this string object
     * at the given index.
     * Given string is not needed to be null terminated.
     *
     * @param string This string object.
     * @param tobeadded C style string to be inserted.
     * @param at Index where the given string will be inserted.
     * @param size Number of bytes to be inserted from the given string.
     * @return New size of this string object.
     */
    ssize_t (*InsertNString)(
            CMUTIL_String *string,
            const char *tobeadded, uint32_t at, size_t size);

    /**
     * Insert a formatted string to this string object.
     *
     * Insert a formatted string to this string object at the index.
     *
     * @param string This string object.
     * @param idx Index where a formatted string will be inserted.
     * @param fmt Format string like a printf function.
     * @param ... Arguments for format string.
     * @return New size of this string object.
     */
    ssize_t (*InsertPrint)(
            CMUTIL_String *string, uint32_t idx, const char *fmt, ...);

    /**
     * Insert a formatted string to this string object.
     *
     * Insert a formatted string to this string object at the index.
     *
     * @param string This string object.
     * @param idx Index where a formatted string will be inserted.
     * @param fmt Format string like a vprintf function.
     * @param args Arguments list for format string.
     * @return New size of this string object.
     */
    ssize_t (*InsertVPrint)(
            CMUTIL_String *string, uint32_t idx,
            const char *fmt, va_list args);

    /**
     * @brief Insert another string object to this string object.
     *
     * Insert the given string object to this string object at the index.
     *
     * @param string This string object.
     * @param idx Index where another string object will be inserted.
     * @param tobeadded Another string object to be inserted.
     * @return New size of this string object.
     */
    ssize_t (*InsertAnother)(
            CMUTIL_String *string, uint32_t idx, CMUTIL_String *tobeadded);

    /**
     * @brief Cut off the given number of characters
     * from the tail of this string.
     *
     * If the length is bigger than the size of this string,
     * this string will be cleared.
     *
     * @param string This string object.
     * @param length Number of characters to be cut off from the tail.
     */
    void (*CutTailOff)(
            CMUTIL_String *string, size_t length);

    /**
     * @brief Get substring from this string object.
     *
     * @param string This string object.
     * @param offset Starting offset of substring.
     * @param length Length of substring.
     * @return New string object which is the substring of this string. Must be
     *         destroyed after use.
     */
    CMUTIL_String *(*Substring)(
            const CMUTIL_String *string, uint32_t offset, size_t length);

    /**
     * @brief Creates a lower-case version of this string.
     *
     * @param string This string object.
     * @return New string object which is the lower case version of this
     *         string. Must be destroyed after use.
     */
    CMUTIL_String *(*ToLower)(
            const CMUTIL_String *string);

    /**
     * @brief Converts this string to lower case.
     *
     * @param string This string object.
     */
    void (*SelfToLower)(
            CMUTIL_String *string);

    /**
     * @brief Creates an upper-case version of this string.
     *
     * @param string This string object.
     * @return New string object which is the upper case version of this
     *         string. Must be destroyed after use.
     */
    CMUTIL_String *(*ToUpper)(
            const CMUTIL_String *string);

    /**
     * @brief Converts this string to the upper case.
     *
     * @param string This string object.
     */
    void (*SelfToUpper)(
            CMUTIL_String *string);

    /**
     * @brief Replace all occurrences of the given substring with another string.
     *
     * @param string This string object.
     * @param needle Substring to be replaced.
     * @param alter Replacement string.
     * @return New string object which is the result of replacement.
     *         Must be destroyed after use.
     */
    CMUTIL_String *(*Replace)(
            const CMUTIL_String *string,
            const char *needle, const char *alter);

    /**
     * @brief Get the size of this string object.
     *
     * @param string This string object.
     * @return Size of this string object.
     */
    size_t (*GetSize)(
            const CMUTIL_String *string);

    /**
     * @brief Get c-style null terminated string from this string object.
     *
     * @param string This string object.
     * @return C-style null terminated string.
     */
    const char *(*GetCString)(
            const CMUTIL_String *string);

    /**
     * @brief Function pointer to retrieve a character from a specified position in a string.
     *
     * This function pointer allows access to a character within a string structure
     * at the position referenced by the provided index.
     *
     * @param string Pointer to a CMUTIL_String structure containing the string data.
     * @param idx The position of the desired character within the string.
     * @return The character located at the specified position, or an negative
     *         value if the index is out of bounds.
     */
    int (*GetChar)(
            const CMUTIL_String *string, uint32_t idx);

    /**
     * @brief Clear the content of this string object.
     *
     * @param string This string object.
     */
    void (*Clear)(
            CMUTIL_String *string);

    /**
     * @brief Clone this string object.
     *
     * @param string This string object.
     * @return New string object which is the clone of this string.
     *         Must be destroyed after use.
     */
    CMUTIL_String *(*Clone)(
            const CMUTIL_String *string);

    /**
     * @brief Destroy this string object.
     *
     * @param string This string object.
     */
    void (*Destroy)(
            CMUTIL_String *string);

    /**
     * @brief Trim spaces from both ends of this string object.
     *
     * @param string This string object.
     */
    void (*SelfTrim)(CMUTIL_String *string);
};

#define CMUTIL_STRING_DEFAULT   32

/**
 * @brief Creates a string object.
 *
 * @return A new string object.
 */
#define CMUTIL_StringCreate()   \
        CMUTIL_StringCreateEx(CMUTIL_STRING_DEFAULT, NULL)

/**
 * @brief Creates a string object with initial capacity and content.
 *
 * @param initcapacity Initial capacity of this string object.
 * @param initcontent Initial content of this string object.
 * @return A new string object.
 */
CMUTIL_API CMUTIL_String *CMUTIL_StringCreateEx(
        size_t initcapacity,
        const char *initcontent);


/**
 * @typedef CMUTIL_StringArray Dynamic array of string objects.
 */
typedef struct CMUTIL_StringArray CMUTIL_StringArray;
struct CMUTIL_StringArray {
    /**
     * @brief Add a string object to this array.
     *
     * Note that ownership of the string object will be moved to this array object.
     * So destroying the original string will lead to undefined behavior.
     * Destroying this array object will destroy all owned strings.
     *
     * @param array This string array object.
     * @param string String object to be added, ownership of the string object
     *          will be moved to this array object.
     */
    void (*Add)(
            CMUTIL_StringArray *array, CMUTIL_String *string);

    /**
     * @brief Add a new c-style string.
     *
     * This method will create a new <code>CMUTIL_String</code> object whose
     * contents will be the same as the given <code>string</code>.
     *
     * @param array This string array object.
     * @param string C-style (NULL-terminated) string to be added.
     */
    void (*AddCString)(
            CMUTIL_StringArray *array, const char *string);

    /**
     * @brief Insert a string object to this array at the index.
     *
     * Note that ownership of the string object will be moved to this array object.
     * So destroying the original string will lead to undefined behavior.
     * Destroying this array object will destroy all owned strings.
     *
     * @param array This string array object.
     * @param string String object to be added, ownership of the string object
     *          will be moved to this array object if CMTrue returned.
     * @param index The index where the new string will be inserted.
     * @return CMTrue if insertion performed successfully.
     *          CMFalse if failed, the ownership of string did not move.
     */
    CMBool (*InsertAt)(
            CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index);

    /**
     * @brief Insert a new c-style string at the given index.
     *
     * This method will create a new <code>CMUTIL_String</code> object whose
     * contents will be the same as given <code>string</code>.
     *
     * @param array This string array object.
     * @param string C-style (NULL-terminated) string to be added.
     * @param index The index where the new string will be inserted.
     * @return CMTrue if insertion performed successfully.
     *          CMFalse if failed.
     */
    CMBool (*InsertAtCString)(
            CMUTIL_StringArray *array, const char *string, uint32_t index);

    /**
     * @brief Remove string element at given index, and return it.
     *
     * Note, this method will transfer ownership
     * of the result string to the caller.
     * The caller must receive the return value and destroy it after use.
     *
     * @param array This string array object.
     * @param index The index where the string wanted to remove.
     * @return String object which removed from this array, ownership of the
     *          string will be transferred to caller.
     *          NULL if the index is out of bound.
     */
    CMUTIL_String *(*RemoveAt)(
            CMUTIL_StringArray *array, uint32_t index);

    /**
     * @brief Set the new string object to this array at given index
     * and old string will be returned.
     *
     * Note that ownership of the string object will be moved to this array object.
     * So destroying the original string will lead to undefined behavior.
     * Destroying this array object will destroy all owned strings.
     * And this method will transfer ownership of the result string to the caller.
     * The caller must receive the return value and destroy it after use.
     *
     * @param array This string array object.
     * @param string String object to be added, ownership of the
     *          string object will be moved to this array object
     *          if CMTrue returned.
     * @param index The index where the string wanted to remove.
     * @return String object which removed from this array, ownership of the
     *          string will be transferred to caller.
     *          NULL if the index is out of bound.
     */
    CMUTIL_String *(*SetAt)(
            CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index);

    /**
     * @brief Set a new c-style string to this array at given index
     *          and old string will be returned.
     *
     * Note this method will transfer ownership of the result string to the caller.
     * The caller must receive the return value and destroy it after use.
     *
     * @param array This string array object.
     * @param string C-style (NULL-terminated) string to be added.
     * @param index The index where the string wanted to remove.
     * @return String object which removed from this array, ownership of the
     *          string will be transferred to caller.
     *          NULL if the index is out of bound.
     */
    CMUTIL_String *(*SetAtCString)(
            CMUTIL_StringArray *array, const char *string, uint32_t index);

    /**
     * @brief Get string object at given index.
     *
     * Note, this method does not transfer ownership of
     * the result string to the caller.
     * The caller must not destroy the result object.
     *
     * @param array This string array object.
     * @param index The index where the string wanted to get.
     * @return Result string object. NULL if the index is out of bound.
     */
    const CMUTIL_String *(*GetAt)(
            const CMUTIL_StringArray *array, uint32_t index);

    /**
     * @brief Get c-style string at the given index.
     *
     * @param array This string array object.
     * @param index The index where the string wanted to get.
     * @return Result c-style string object. NULL if the index is out of bound.
     */
    const char *(*GetCString)(
            const CMUTIL_StringArray *array, uint32_t index);

    /**
     * @brief Get the number of items in this array.
     *
     * @param array This string array object.
     * @return The size of this array.
     */
    size_t (*GetSize)(
            const CMUTIL_StringArray *array);

    /**
     * @brief Get the iterator of this array.
     *
     * @param array This string object.
     * @return Iterator object of this array must be destroyed after use.
     */
    CMUTIL_Iterator *(*Iterator)(
            const CMUTIL_StringArray *array);

    /**
     * @brief Destroy this array object and it's contents.
     *
     * @param array This string object.
     */
    void (*Destroy)(
            CMUTIL_StringArray *array);
};

/**
 * Default initial size of CMUTIL_String object.
 */
#define CMUTIL_STRINGARRAY_DEFAULT  32

/**
 * Creates a new CMUTIL_String object.
 * @return A new CMUTIL_String object.
 */
#define CMUTIL_StringArrayCreate()  \
        CMUTIL_StringArrayCreateEx(CMUTIL_STRINGARRAY_DEFAULT)

/**
 * Creates a new CMUTIL_String object with initial capacity.
 *
 * @param initcapacity Initial capacity of the new CMUTIL_String object.
 * @return A new CMUTIL_String object.
 */
CMUTIL_API CMUTIL_StringArray *CMUTIL_StringArrayCreateEx(
        size_t initcapacity);

/**
 * @brief Trim the right side of a c-style string.
 *
 * Trailing space characters (space, tab, line-feed, carriage-return) are
 * removed from the input string.
 * Input string must be null-terminated,
 * and its contents could be changed after this function call.
 * The result string reference is pointing to the original input string.
 *
 * @param inp Input string to be trimmed.
 * @return Right "trimmed" input string (pointing same as input).
 */
CMUTIL_API char *CMUTIL_StrRTrim(char *inp);

/**
 * @brief Trim the left side of a c-style string.
 *
 * Leading space characters (space, tab, line-feed, carriage-return) are
 * removed from the input string.
 * Input string must be null-terminated,
 * and its contents could be changed after this function call.
 * The result string reference is pointing to the original input string.
 *
 * @param inp Input string to be trimmed.
 * @return Right "trimmed" input string (pointing same as input).
 */
CMUTIL_API char *CMUTIL_StrLTrim(char *inp);

/**
 * @brief Trim the both side of a c-style string.
 *
 * Leading and trailing space characters (space, tab, line-feed, carriage-return)
 * are removed from the input string.
 * Input string must be null-terminated,
 * and its contents could be changed after this function call.
 * The result string reference is pointing to the original input string.
 *
 * @param inp Input string to be trimmed.
 * @return Left and right "trimmed" input string (pointing same as input).
 */
CMUTIL_API char *CMUTIL_StrTrim(char *inp);

/**
 * @brief Get the next token from src.
 *
 * If buflen is less than the length of the next token, the token will be split
 * into multiple tokens.
 *
 * @param dest The next token will be stored in here.
 * @param buflen Maximum length of <code>dest</code>.
 * @param src The source string to get the next token.
 * @param delims Delimiters to determine token.
 * @return The position of src after the token. NULL if invalid parameter.
 */
CMUTIL_API const char *CMUTIL_StrNextToken(
    char *dest, size_t buflen, const char *src, const char *delims);

/**
 * @brief Skip leading spaces.
 *
 * Remove any leading space characters, custom <code>spaces</code>
 * can be supplied.
 *
 * @param src Source string.
 * @param spaces Any characters in here will be regards to space.
 * @return The position in <code>src</code> after leading spaces.
 */
CMUTIL_API const char *CMUTIL_StrSkipSpaces(
        const char *src, const char *spaces);

/**
 * @brief Split the <code>haystack</code> string using the <code>needle</code>.
 *
 * Any occurrence of the <code>needle</code> in the <code>haystack</code>
 * will cause a split.
 *
 * @param haystack Source string.
 * @param needle Split delimiter.
 * @return Result strings in <code>CMUTIL_StringArray</code> form.
 */
CMUTIL_API CMUTIL_StringArray *CMUTIL_StringSplit(
    const char *haystack, const char *needle);

/**
 * @brief Convert hex encoded string to bytes.
 *
 * If <code>len</code> is an odd number, pad a '0' at the beginning.
 *
 * @param dest Byte array where result bytes will be stored in.
 * @param src Hex encoded source string.
 * @param len The number of characters to be converted from the source string.
 * @return Number of bytes of results.
 */
CMUTIL_API int CMUTIL_StringHexToBytes(uint8_t *dest, const char *src, int len);


/**
 * @typedef CMUTIL_ByteBuffer Manipulation of bytes.
 */
typedef struct CMUTIL_ByteBuffer CMUTIL_ByteBuffer;
struct CMUTIL_ByteBuffer {

    /**
     * @brief Append a byte to this buffer.
     *
     * @param buffer This buffer object.
     * @param b A byte to be added.
     * @return This buffer object. NULL if the addition is failed.
     */
    CMUTIL_ByteBuffer *(*AddByte)(
            CMUTIL_ByteBuffer *buffer,
            uint8_t b);

    /**
     * @brief Adds a sequence of bytes to the specified buffer.
     *
     * This function appends a provided array of bytes to the end of the given
     * byte buffer. It also updates the buffer's internal state to include the
     * newly added bytes.
     *
     * @param buffer A pointer to the byte buffer where the bytes will be added.
     * @param bytes A pointer to the array of bytes to be appended.
     * @param length The number of bytes to be added from the array.
     * @return A pointer to the updated buffer if the operation is successful,
     *         or NULL if the addition fails.
     */
    CMUTIL_ByteBuffer *(*AddBytes)(
            CMUTIL_ByteBuffer *buffer,
            const uint8_t *bytes,
            uint32_t length);

    /**
     * @brief Adds a portion of a byte array to the specified buffer.
     *
     * This function appends a subset of the provided byte array to the specified
     * byte buffer. The portion of the array to be appended is determined by the
     * provided offset and length. The function updates the buffer's state to
     * reflect the added bytes.
     *
     * @param buffer A pointer to the byte buffer where the bytes will be added.
     * @param bytes A pointer to the source byte array.
     * @param offset The starting position within the source array from which bytes
     *               will be copied.
     * @param length The number of bytes to add from the source array.
     * @return A pointer to the updated buffer if the operation is successful,
     *         or NULL if the addition fails.
     */
    CMUTIL_ByteBuffer *(*AddBytesPart)(
            CMUTIL_ByteBuffer *buffer,
            const uint8_t *bytes,
            uint32_t offset,
            uint32_t length);

    /**
     * @brief Inserts a byte at a specified index in the byte buffer.
     *
     * This function allows placing a single byte at any specified position within
     * the given byte buffer. The existing data in the buffer is shifted as needed
     * to make space for the new byte. If the insertion fails, the buffer remains
     * unmodified.
     *
     * @param buffer A pointer to the byte buffer where the byte will be inserted.
     * @param index The position in the buffer at which the byte should be inserted.
     * @param b The byte to insert into the buffer.
     * @return A pointer to the updated byte buffer if the operation is successful,
     *         or NULL if the insertion fails.
     */
    CMUTIL_ByteBuffer *(*InsertByteAt)(
            CMUTIL_ByteBuffer *buffer,
            uint32_t index,
            uint8_t b);

    /**
     * @brief Inserts a sequence of bytes at a specified index in the byte buffer.
     *
     * This function allows inserting an array of bytes into a specific position
     * within the given byte buffer. The existing data in the buffer is shifted
     * as needed to make space for the new bytes. If the insertion operation fails,
     * the buffer remains unmodified.
     *
     * @param buffer A pointer to the byte buffer where the bytes will be inserted.
     * @param index The position in the buffer at which the bytes should be inserted.
     * @param bytes A pointer to the array of bytes to insert into the buffer.
     * @param length The number of bytes to insert from the array.
     * @return A pointer to the updated byte buffer if the operation is successful,
     *         or NULL if the insertion fails.
     */
    CMUTIL_ByteBuffer *(*InsertBytesAt)(
            CMUTIL_ByteBuffer *buffer,
            uint32_t index,
            const uint8_t *bytes,
            uint32_t length);

    /**
     * @brief Retrieves the byte at a specific index in the byte buffer.
     *
     * This function provides access to the byte located at a given index within
     * the specified byte buffer without modifying the buffer. If the index is
     * out of bounds, it returns a negative value.
     *
     * @param buffer A pointer to the byte buffer from which the byte should be retrieved.
     * @param index The position of the byte to retrieve within the buffer.
     * @return The byte located at the specified index in the buffer,
     *         a negative value if failed.
     */
    int (*GetAt)(
            const CMUTIL_ByteBuffer *buffer,
            uint32_t index);

    /**
     * @brief Retrieves the size of the given byte buffer.
     *
     * This function returns the total number of bytes currently stored
     * in the specified byte buffer. It provides a way to determine
     * the current capacity or length of the buffer's contents.
     *
     * @param buffer A pointer to the byte buffer whose size will be retrieved.
     * @return The size of the byte buffer, in bytes.
     */
    size_t (*GetSize)(
            const CMUTIL_ByteBuffer *buffer);

    /**
     * @brief Function pointer to retrieve a byte array from a CMUTIL_ByteBuffer.
     *
     * This variable is a function pointer that, when assigned, points to a
     * function capable of extracting a byte array stored in the specified
     * CMUTIL_ByteBuffer structure.
     *
     * @param buffer Pointer to a CMUTIL_ByteBuffer structure from
     *               which the byte array is to be retrieved.
     * @return Pointer to the byte array extracted from the given buffer.
     */
    uint8_t *(*GetBytes)(
            CMUTIL_ByteBuffer *buffer);

    /**
     * @brief Function pointer for resizing a CMUTIL_ByteBuffer to a specified size.
     *
     * This function reduces the size of the provided byte buffer to the specified
     * size if possible. Success or failure is indicated through the returned boolean value.
     *
     * @param buffer A pointer to the CMUTIL_ByteBuffer to be resized.
     * @param size The target size to shrink the buffer to.
     * @return A CMBool indicating whether the operation was successful.
     */
    CMBool (*ShrinkTo)(
            CMUTIL_ByteBuffer *buffer,
            size_t size);

    /**
     * @brief Retrieves the total capacity of the specified byte buffer.
     *
     * This function pointer returns the maximum number of bytes the given
     * CMUTIL_ByteBuffer can hold without resize operation.
     *
     * @param buffer A pointer to the CMUTIL_ByteBuffer instance.
     * @return The capacity of the byte buffer in bytes.
     */
    size_t (*GetCapacity)(
            const CMUTIL_ByteBuffer *buffer);

    /**
     * @brief Function pointer for destroying a CMUTIL_ByteBuffer instance.
     *
     * This function is responsible for releasing and cleaning up resources
     * associated with a given CMUTIL_ByteBuffer object.
     *
     * @param buffer A pointer to the CMUTIL_ByteBuffer instance to be destroyed.
     */
    void (*Destroy)(
            CMUTIL_ByteBuffer *buffer);

    /**
     * @brief Function pointer for clearing the contents of a CMUTIL_ByteBuffer.
     *
     * This function is used to reset or clear the data in a provided byte buffer,
     * ensuring that its contents are erased or set back to an initial state.
     *
     * @param buffer A pointer to the CMUTIL_ByteBuffer to be cleared.
     */
    void (*Clear)(
            CMUTIL_ByteBuffer *buffer);
};

/**
 * Default bytebuffer initial capacity.
 */
#define CMUTIL_BYTEBUFFER_DEFAULT   32

/**
 * Creates CMUTIL_ByteBuffer object with the default parameter.
 */
#define CMUTIL_ByteBufferCreate()   \
        CMUTIL_ByteBufferCreateEx(CMUTIL_BYTEBUFFER_DEFAULT)

/**
 * @brief Creates CMUTIL_ByteBuffer object with initial capacity.
 *
 * @param initcapacity Initial capacity of this bytebuffer.
 * @return A new bytebuffer object.
 */
CMUTIL_API CMUTIL_ByteBuffer *CMUTIL_ByteBufferCreateEx(
        size_t initcapacity);


/**
 * @typedef CMUTIL_Map Hashmap type with item order preserved.
 */
typedef struct CMUTIL_Map CMUTIL_Map;
struct CMUTIL_Map {
    /**
     * @brief Inserts a key-value pair into the map.
     *
     * This function adds a new entry into the map with the specified key and value.
     * If the key already exists in the map, the associated value is updated to the
     * new value provided. The function returns a pointer to the previous value
     * associated with the key if it existed, or NULL if the key was not found in
     * the map before the operation.
     * The order of the items in the map is preserved. If the key already exists,
     * the previous order is discarded and the new key-value pair is inserted at
     * the end of the map.
     * If 70% of buckets are occupied, the map is rebuilt with 2 times
     * the current size to reduce the collision rate.
     *
     * @param map A pointer to the map where the key-value pair should be inserted.
     * @param key The key associated with the value to insert or update in the map.
     * @param value A pointer to the value to associate with the specified key.
     * @return A pointer to the previous value associated with the key, or nullptr
     *         if the key did not previously exist in the map.
     */
    void *(*Put)(
            CMUTIL_Map *map, const char* key, void* value);

    /**
     * @brief Copies all key-value pairs from one map to another.
     *
     * This function transfers all key-value pairs from the source map to the
     * target map. If keys in the source map already exist in the target map,
     * their values will be replaced with the values from the source map.
     * The operation does not clear existing entries in the target map that
     * do not have corresponding keys in the source map.
     *
     * @param map A pointer to the target map where the key-value pairs will be copied.
     * @param src A pointer to the source map containing the key-value pairs to copy.
     */
    void (*PutAll)(
            CMUTIL_Map *map, const CMUTIL_Map *src);

    /**
     * @brief Retrieves the value associated with a given key in the map.
     *
     * This function searches the map for the specified key and returns the
     * value associated with it. If the key is not found, the function
     * returns a null pointer.
     *
     * @param map A pointer to the map object to search.
     * @param key The key whose associated value is to be retrieved.
     * @return A pointer to the value associated with the given key, or NULL
     *         if the key is not found.
     */
    void *(*Get)(
            const CMUTIL_Map *map, const char* key);

    /**
     * @brief Removes the key-value pair associated with the given key from the map.
     *
     * This function searches the map for the specified key and removes the
     * corresponding key-value pair if found. If the key is not found, the
     * function does nothing and returns NULL.
     *
     * @param map A pointer to the map object from which to remove the key-value pair.
     * @param key The key to be removed from the map.
     * @return A pointer to the value associated with the removed key, or NULL
     *         if the key was not found.
     */
    void *(*Remove)(
            CMUTIL_Map *map, const char* key);

    /**
     * @brief Get the keys of the map.
     *
     * This function returns a string array containing all the keys present
     * in the map.
     * The caller is responsible for freeing the returned string array using
     * the Destroy method.
     *
     * @param map A pointer to the map object from which to retrieve the keys.
     * @return A pointer to a string array containing the keys
     *         or nullptr if the map is empty.
     */
    CMUTIL_StringArray *(*GetKeys)(
            const CMUTIL_Map *map);

    /**
     * @brief Get the size of the map.
     *
     * This function returns the number of key-value pairs currently stored
     * in the map.
     *
     * @param map A pointer to the map object from which to retrieve the size.
     * @return The number of key-value pairs in the map.
     */
    size_t (*GetSize)(
            const CMUTIL_Map *map);

    /**
     * @brief Get iterator for the map.
     *
     * This function returns an iterator object that can be used to traverse
     * the values in the map. The caller is responsible for freeing
     * the returned iterator using the Destroy method.
     *
     * @param map A pointer to the map object from which to retrieve the iterator.
     * @return A pointer to the iterator object, or NULL if the map is empty.
     */
    CMUTIL_Iterator *(*Iterator)(
            const CMUTIL_Map *map);

    /**
     * @brief Clear the map.
     *
     * This function removes all key-value pairs from the map, effectively
     * resetting it to an empty state. The function does not free the memory
     * associated with the map itself, only the key-value pairs.
     * If this map is created with freecb, it will be called freecb for each value.
     * Otherwise, it will not free the values.
     *
     * @param map A pointer to the map object to be cleared.
     */
    void (*Clear)(
            CMUTIL_Map *map);

    /**
     * @brief Clear the map and free all key-value pairs.
     *
     * This function removes all key-value pairs from the map, effectively
     * resetting it to an empty state. The function will free the memory
     * associated with the map itself and all key-value pairs.
     * Whether this map is created with or without freecb,
     * it will not free the values.
     *
     * @param map A pointer to the map object to be cleared.
     */
    void (*ClearLink)(
            CMUTIL_Map *map);

    /**
     * @brief Destroy the map object.
     *
     * This function frees the memory associated with the map object and
     * all key-value pairs. If the map was created with a freecb function,
     * it will be called for each value to free any additional memory.
     *
     * @param map A pointer to the map object to be destroyed.
     */
    void (*Destroy)(
            CMUTIL_Map *map);

    /**
     * @brief Print the map to a string.
     *
     * This function appends a string representation of the map to the
     * provided CMUTIL_String object. The format of the string is
     * implementation-dependent.
     *
     * @param map A pointer to the map object to be printed.
     * @param out A pointer to the CMUTIL_String object to append the output.
     */
    void (*PrintTo)(
            const CMUTIL_Map *map, CMUTIL_String *out);

    /**
     * @brief Get the value at a specific index in the map.
     *
     * This function retrieves the value at the specified index in the map.
     * The index is zero-based, with 0 being the first element.
     *
     * @param map A pointer to the map object from which to retrieve the value.
     * @param index The index of the value to retrieve.
     * @return A pointer to the value at the specified index,
     *         or NULL if the index is out of bounds.
     */
    void *(*GetAt)(
            const CMUTIL_Map *map, uint32_t index);

    /**
     * @brief Remove the value at a specific index in the map.
     *
     * This function removes the value at the specified index in the map.
     * The index is zero-based, with 0 being the first element.
     *
     * @param map A pointer to the map object from which to remove the value.
     * @param index The index of the value to remove.
     * @return A pointer to the removed value,
     *         or NULL if the index is out of bounds.
     */
    void *(*RemoveAt)(
            CMUTIL_Map *map, uint32_t index);
};

/**
 * The default initial bucket size for the map.
 */
#define CMUTIL_MAP_DEFAULT  256

/**
 * Create a new map with default settings.
 */
#define CMUTIL_MapCreate()  CMUTIL_MapCreateEx(\
        CMUTIL_MAP_DEFAULT, CMFalse, NULL)

/**
 * @brief Create a new map with custom settings.
 * @param bucketsize The initial number of buckets for the map.
 * @param isucase Whether the key of the map should be case-insensitive.
 * @param freecb A callback function to free values when the map is destroyed.
 * @return A pointer to the newly created map, or NULL on failure.
 */
CMUTIL_API CMUTIL_Map *CMUTIL_MapCreateEx(
        uint32_t bucketsize, CMBool isucase, CMFreeCB freecb);


/**
 * @typedef CMUTIL_List A doubly linked list type.
 */
typedef struct CMUTIL_List CMUTIL_List;
struct CMUTIL_List {

    /**
     * @brief Add an item to the front of the list.
     *
     * This function inserts a new item at the beginning of the list.
     * Ownership of the data is transferred to the list.
     *
     * @param list The list to which the item will be added.
     * @param data The data to be added to the front of the list.
     */
    void (*AddFront)(CMUTIL_List *list, void *data);

    /**
     * @brief Add an item to the tail of the list.
     *
     * This function appends a new item at the end of the list.
     * Ownership of the data is transferred to the list.
     *
     * @param list The list to which the item will be added.
     * @param data The data to be added to the tail of the list.
     */
    void (*AddTail)(CMUTIL_List *list, void *data);

    /**
     * @brief Get the item at the front of the list.
     *
     * This function retrieves the data at the front of the list without removing it.
     * Ownership of the data remains with the list.
     *
     * @param list The list from which the item will be retrieved.
     * @return The data at the front of the list, or NULL if the list is empty.
     */
    void *(*GetFront)(const CMUTIL_List *list);

    /**
     * @brief Get the item at the tail of the list.
     *
     * This function retrieves the data at the end of the list without removing it.
     * Ownership of the data remains with the list.
     *
     * @param list The list from which the item will be retrieved.
     * @return The data at the tail of the list, or NULL if the list is empty.
     */
    void *(*GetTail)(const CMUTIL_List *list);

    /**
     * @brief Remove and return the item at the front of the list.
     *
     * This function removes the item at the beginning of the list and returns its data.
     * Ownership of the data is transferred to the caller.
     *
     * @param list The list from which the item will be removed.
     * @return The data of the removed item, or NULL if the list is empty.
     */
    void *(*RemoveFront)(CMUTIL_List *list);

    /**
     * @brief Remove and return the item at the tail of the list.
     *
     * This function removes the item at the end of the list and returns its data.
     * Ownership of the data is transferred to the caller.
     *
     * @param list The list from which the item will be removed.
     * @return The data of the removed item, or NULL if the list is empty.
     */
    void *(*RemoveTail)(CMUTIL_List *list);

    /**
     * @brief Remove a specific item from the list.
     *
     * This function searches for the specified data in the list, removes it if found,
     * and returns the data. Ownership of the data is transferred to the caller.
     *
     * @param list The list from which the item will be removed.
     * @param data The data to be removed from the list.
     * @return The data of the removed item, or NULL if the item was not found
     */
    void *(*Remove)(CMUTIL_List *list, void *data);

    /**
     * @brief Get the size of the list.
     *
     * This function returns the number of items currently in the list.
     *
     * @param list The list whose size will be retrieved.
     * @return The number of items in the list.
     */
    size_t (*GetSize)(const CMUTIL_List *list);

    /**
     * @brief Get an iterator for the list.
     *
     * This function returns an iterator that can be used to traverse the items in the list.
     * The caller is responsible for destroying the iterator after use.
     *
     * @param list The list for which the iterator will be created.
     * @return An iterator for the list.
     */
    CMUTIL_Iterator *(*Iterator)(const CMUTIL_List *list);

    /**
     * @brief Destroy the list and free its resources.
     *
     * This function frees all resources associated with the list.
     * If the list was created with a freecb function, it will be called
     * for each item in the list to free any additional memory.
     *
     * @param list The list to be destroyed.
     */
    void (*Destroy)(CMUTIL_List *list);

    /**
     * @brief Move all items from one list to another.
     *
     * This function moves all items from the source list to the destination list.
     * The source list will be empty after the operation.
     *
     * @param dst The destination list to which items will be moved.
     * @param src The source list from which items will be moved.
     */
    void (*MoveAll)(CMUTIL_List *dst, CMUTIL_List *src);
};

/**
 * @brief Create a new list with default settings.
 *
 * This macro creates a new CMUTIL_List object without a custom free callback.
 *
 * @return A pointer to the newly created list, or NULL on failure.
 */
#define CMUTIL_ListCreate() CMUTIL_ListCreateEx(NULL)

/**
 * @brief Create a new list with a custom free callback.
 *
 * This function creates a new CMUTIL_List object with a specified
 * callback function to free items when the list is destroyed.
 *
 * @param freecb A callback function to free items when the list is destroyed.
 *               If NULL, items will not be freed.
 * @return A pointer to the newly created list, or NULL on failure.
 */
CMUTIL_API CMUTIL_List *CMUTIL_ListCreateEx(CMFreeCB freecb);




/**
 * @typedef CMXmlNodeKind The kind of XML node.
 */
typedef enum CMXmlNodeKind {
    /** Unknown node type. */
    CMXmlNodeUnknown = 0,
    /** Text node type. */
    CMXmlNodeText,
    /** XML tag node type. */
    CMXmlNodeTag
} CMXmlNodeKind;


/**
 * @typedef CMUTIL_XmlNode XML node manipulation.
 */
typedef struct CMUTIL_XmlNode CMUTIL_XmlNode;
struct CMUTIL_XmlNode {
    /**
     * @brief Get all attribute names of this node.
     *
     * The caller is responsible for freeing the returned string array.
     *
     * @param node This XML node object.
     * @return A string array containing all attribute names.
     */
    CMUTIL_StringArray *(*GetAttributeNames)(const CMUTIL_XmlNode *node);

    /**
     * @brief Get the value of an attribute by key.
     *
     * The caller is responsible for freeing the returned string.
     *
     * @param node This XML node object.
     * @param key The attribute key.
     * @return The value of the attribute as a string, or NULL if not found.
     */
    CMUTIL_String *(*GetAttribute)(
            const CMUTIL_XmlNode *node, const char *key);

    /**
     * @brief Set the value of an attribute by key.
     *
     * @param node This XML node object.
     * @param key The attribute key.
     * @param value The value to set for the attribute.
     */
    void (*SetAttribute)(
            CMUTIL_XmlNode *node, const char *key, const char *value);

    /**
     * @brief Get the number of child nodes.
     *
     * @param node This XML node object.
     * @return The number of child nodes.
     */
    size_t (*ChildCount)(const CMUTIL_XmlNode *node);

    /**
     * @brief Add a child node to this node.
     *
     * The ownership of the child node is transferred to this node.
     *
     * @param node This XML node object.
     * @param child The child node to add.
     */
    void (*AddChild)(CMUTIL_XmlNode *node, CMUTIL_XmlNode *child);

    /**
     * @brief Get the child node at a specific index.
     *
     * The caller is responsible for not modifying or destroying
     * the returned child node.
     *
     * @param node This XML node object.
     * @param index The index of the child node to retrieve.
     * @return The child node at the specified index,
     *         or NULL if index is out of bounds.
     */
    CMUTIL_XmlNode *(*ChildAt)(const CMUTIL_XmlNode *node, uint32_t index);

    /**
     * @brief Get the parent node of this node.
     *
     * The caller is responsible for not modifying or destroying
     * the returned parent node.
     *
     * @param node This XML node object.
     * @return The parent node, or NULL if this node has no parent.
     */
    CMUTIL_XmlNode *(*GetParent)(const CMUTIL_XmlNode *node);

    /**
     * @brief Get the name of this node.
     *
     * @return The name of the node as a C-style string.
     */
    const char *(*GetName)(const CMUTIL_XmlNode *node);

    /**
     * @brief Set the name of this node.
     *
     * @param node This XML node object.
     * @param name The new name for the node.
     */
    void (*SetName)(CMUTIL_XmlNode *node, const char *name);

    /**
     * @brief Get the text content of this node.
     *
     * The caller is responsible for freeing the returned string.
     *
     * @param node This XML node object.
     * @return The text content of the node as a string.
     */
    CMXmlNodeKind (*GetType)(const CMUTIL_XmlNode *node);

    /**
     * @brief Convert this node and its children to an XML document string.
     *
     * The caller is responsible for freeing the returned string.
     *
     * @param node This XML node object.
     * @param beutify Whether to format the output string for readability.
     * @return The XML document string representation of this node.
     */
    CMUTIL_String *(*ToDocument)(
            const CMUTIL_XmlNode *node, CMBool beutify);

    /**
     * @brief Set user data associated with this node.
     *
     * The user data can be any pointer type. A callback function
     * can be provided to free the user data when the node is destroyed.
     *
     * @param node This XML node object.
     * @param udata The user data to associate with this node.
     * @param freef A callback function to free the user data when no longer needed.
     */
    void (*SetUserData)(
            CMUTIL_XmlNode *node, void *udata, void(*freef)(void*));

    /**
     * @brief Get the user data associated with this node.
     *
     * @param node This XML node object.
     * @return The user data associated with this node.
     */
    void *(*GetUserData)(const CMUTIL_XmlNode *node);

    /**
     * @brief Destroy this XML node and free its resources.
     *
     * @param node This XML node object.
     */
    void (*Destroy)(CMUTIL_XmlNode *node);
};

/**
 * @brief Parse an XML string into an XML node tree.
 *
 * The caller is responsible for destroying the returned XML node tree
 * using the Destroy method of CMUTIL_XmlNode.
 *
 * @param str The XML string to parse.
 * @return The root XML node of the parsed tree, or NULL on failure.
 */
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParse(CMUTIL_String *str);

/**
 * @brief Parse an XML string into an XML node tree.
 *
 * The caller is responsible for destroying the returned XML node tree
 * using the Destroy method of CMUTIL_XmlNode.
 *
 * @param xmlstr The XML string to parse.
 * @param len The length of the XML string.
 * @return The root XML node of the parsed tree, or NULL on failure.
 */
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseString(
        const char *xmlstr, size_t len);

/**
 * @brief Parse an XML file into an XML node tree.
 *
 * The caller is responsible for destroying the returned XML node tree
 * using the Destroy method of CMUTIL_XmlNode.
 *
 * @param fpath The path to the XML file to parse.
 * @return The root XML node of the parsed tree, or NULL on failure.
 */
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseFile(const char *fpath);

/**
 * @brief Create a new XML node with the specified type and tag name.
 *
 * The caller is responsible for destroying the returned XML node
 * using the Destroy method of CMUTIL_XmlNode.
 *
 * @param type The type of the XML node (e.g., CMXmlNodeTag, CMXmlNodeText).
 * @param tagname The tag name of the XML node.
 * @return The newly created XML node, or NULL on failure.
 */
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreate(
        CMXmlNodeKind type, const char *tagname);

/**
 * @brief Create a new XML node with the specified type and tag name of the given length.
 *
 * The caller is responsible for destroying the returned XML node
 * using the Destroy method of CMUTIL_XmlNode.
 *
 * @param type The type of the XML node (e.g., CMXmlNodeTag, CMXmlNodeText).
 * @param tagname The tag name of the XML node.
 * @param len The length of the tag name.
 * @return The newly created XML node, or NULL on failure.
 */
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
        CMXmlNodeKind type, const char *tagname, size_t len);



/**
 * @typedef CMUTIL_CSConv Character set conversion object.
 */
typedef struct CMUTIL_CSConv CMUTIL_CSConv;
struct CMUTIL_CSConv {
    /**
     * @brief Convert a string from the source character set to the target character set.
     *
     * This function takes an input string encoded in the source character set
     * and converts it to the target character set, returning a new string
     * in the target encoding.
     *
     * @param conv A pointer to the CMUTIL_CSConv object that defines the
     *             source and target character sets.
     * @param instr A pointer to the input string to be converted.
     * @return A pointer to the newly converted string in the target character set.
     */
    CMUTIL_String *(*Forward)(
            const CMUTIL_CSConv *conv, CMUTIL_String *instr);

    /**
     * @brief Convert a string from the target character set back to the source character set.
     *
     * This function takes an input string encoded in the target character set
     * and converts it back to the source character set, returning a new string
     * in the source encoding.
     *
     * @param conv A pointer to the CMUTIL_CSConv object that defines the
     *             source and target character sets.
     * @param instr A pointer to the input string to be converted.
     * @return A pointer to the newly converted string in the source character set.
     */
    CMUTIL_String *(*Backward)(
            const CMUTIL_CSConv *conv, CMUTIL_String *instr);

    /**
     * @brief Destroy the character set conversion object.
     *
     * @param conv A pointer to the CMUTIL_CSConv object to be destroyed.
     */
    void (*Destroy)(CMUTIL_CSConv *conv);
};

/**
 * @brief Create a character set conversion object.
 *
 * @param fromcs The source character set name.
 * @param tocs The target character set name.
 * @return A pointer to the newly created CMUTIL_CSConv object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_CSConv *CMUTIL_CSConvCreate(
        const char *fromcs, const char *tocs);


/**
 * @typedef CMUTIL_TimerTask Timer task handle.
 */
typedef struct CMUTIL_TimerTask CMUTIL_TimerTask;
struct CMUTIL_TimerTask {
    /**
     * @brief Cancel the scheduled timer task and destroy.
     *
     * This function cancels the execution of the timer task
     * if it has not yet been executed. Once canceled, the task
     * will not be executed in the future.
     * This function also frees the resources associated with the timer task.
     * Do not call this function after Timer destroyed,
     * may cause undefined behavior.
     *
     * @param task The timer task to be canceled and destroyed.
     */
    void (*Cancel)(CMUTIL_TimerTask *task);
};

/**
 * @typedef CMUTIL_Timer Timer object for scheduling tasks.
 */
typedef struct CMUTIL_Timer CMUTIL_Timer;
struct CMUTIL_Timer {
    /**
     * @brief Schedule a task to run at a specific time.
     *
     * @param timer The timer object used to schedule the task.
     * @param attime The absolute time at which to run the task.
     * @param proc The callback function to execute when the task is run.
     * @param param A user-defined parameter to pass to the callback function.
     * @return A handle to the scheduled timer task.
     */
    CMUTIL_TimerTask *(*ScheduleAtTime)(
            CMUTIL_Timer *timer, struct timeval *attime,
            CMProcCB proc, void *param);

    /**
     * @brief Schedule a task to run after a specified delay.
     *
     * @param timer The timer object used to schedule the task.
     * @param delay The delay in milliseconds before the task is run.
     * @param proc The callback function to execute when the task is run.
     * @param param A user-defined parameter to pass to the callback function.
     * @return A handle to the scheduled timer task.
     */
    CMUTIL_TimerTask *(*ScheduleDelay)(
            CMUTIL_Timer *timer, long delay,
            CMProcCB proc, void *param);

    /**
     * @brief Schedule a task to run repeatedly at fixed intervals,
     *        starting at a specific time.
     *
     * @param timer The timer object used to schedule the task.
     * @param first The absolute time at which to run the first execution of the task.
     * @param period The interval in milliseconds between subsequent executions of the task.
     * @param elapse_skip If CMTrue, the task will skip elapsed time intervals when it is delayed.
     * @param proc The callback function to execute when the task is run.
     * @param param A user-defined parameter to pass to the callback function.
     * @return A handle to the scheduled timer task.
     */
    CMUTIL_TimerTask *(*ScheduleAtRepeat)(
            CMUTIL_Timer *timer, struct timeval *first, long period,
            CMBool elapse_skip, CMProcCB proc, void *param);

    /**
     * @brief Schedule a task to run repeatedly at fixed intervals,
     *        starting after a specified delay.
     *
     * @param timer The timer object used to schedule the task.
     * @param delay The delay in milliseconds before the first execution of the task.
     * @param period The interval in milliseconds between subsequent executions of the task.
     * @param elapse_skip If CMTrue, the task will skip elapsed time intervals when it is delayed.
     * @param proc The callback function to execute when the task is run.
     * @param param A user-defined parameter to pass to the callback function.
     * @return A handle to the scheduled timer task.
     */
    CMUTIL_TimerTask *(*ScheduleDelayRepeat)(
            CMUTIL_Timer *timer, long delay, long period,
            CMBool elapse_skip, CMProcCB proc, void *param);

    /**
     * @brief Purge all scheduled tasks from the timer.
     *
     * This function removes all tasks that have been scheduled
     * on the timer, effectively clearing the task queue.
     *
     * @param timer The timer object from which to purge tasks.
     */
    void (*Purge)(CMUTIL_Timer *timer);

    /**
     * @brief Destroy the timer object and free its resources.
     *
     * This function stops the timer and releases all associated
     * resources. Any scheduled tasks that have not yet executed
     * will be canceled.
     *
     * @param timer The timer object to be destroyed.
     */
    void (*Destroy)(CMUTIL_Timer *timer);
};

/**
 * Default timer precision in milliseconds.
 */
#define CMUTIL_TIMER_PRECISION  1

/**
 * Default number of threads for the timer.
 */
#define CMUTIL_TIMER_THREAD     2

/**
 * @brief Create a timer object with default settings.
 *
 * This macro creates a timer object using the default precision
 * and number of threads.
 *
 * @return A pointer to the newly created CMUTIL_Timer object,
 *         or NULL on failure.
 */
#define CMUTIL_TimerCreate()    \
        CMUTIL_TimerCreateEx(CMUTIL_TIMER_PRECISION, CMUTIL_TIMER_THREAD)

/**
 * @brief Create a timer object with custom settings.
 *
 * @param precision The precision of the timer in milliseconds.
 * @param threads The number of threads to use for the timer.
 * @return A pointer to the newly created CMUTIL_Timer object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_Timer *CMUTIL_TimerCreateEx(long precision, int threads);


/**
 * @typedef CMUTIL_Pool Resource pool object.
 */
typedef struct CMUTIL_Pool CMUTIL_Pool;
struct CMUTIL_Pool {
    /**
     * @brief Check out a resource from the pool.
     *
     * This function retrieves a resource from the pool, waiting
     * up to the specified number of milliseconds if no resources
     * are currently available.
     *
     * @param pool The resource pool from which to check out a resource.
     * @param millisec The maximum time to wait for a resource, in milliseconds.
     *                  A value of 0 means to return immediately if no resources
     *                  are available. A negative value means to wait indefinitely.
     * @return A pointer to the checked-out resource, or NULL if no resource
     *         could be obtained within the specified time.
     */
    void *(*CheckOut)(CMUTIL_Pool *pool, long millisec);

    /**
     * @brief Release a resource back to the pool.
     *
     * This function returns a previously checked-out resource
     * back to the pool for reuse.
     *
     * @param pool The resource pool to which the resource will be released.
     * @param resource The resource to be released back to the pool.
     */
    void (*Release)(CMUTIL_Pool *pool, void *resource);

    /**
     * @brief Add a new resource to the pool.
     *
     * This function adds a new resource to the pool, increasing
     * the total number of resources managed by the pool.
     *
     * @param pool The resource pool to which the resource will be added.
     * @param resource The resource to be added to the pool.
     */
    void (*AddResource)(CMUTIL_Pool *pool, void *resource);

    /**
     * @brief Destroy the resource pool and free its resources.
     *
     * This function releases all resources managed by the pool
     * and frees any associated memory. After calling this function,
     * the pool object should not be used again.
     *
     * @param pool The resource pool to be destroyed.
     */
    void (*Destroy)(CMUTIL_Pool *pool);
};

/**
 * @typedef CMPoolItemCreateCB Callback type for creating a pool item.
 */
typedef void* (*CMPoolItemCreateCB)(void *udata);

/**
 * @typedef CMPoolItemFreeCB Callback type for freeing a pool item.
 */
typedef void (*CMPoolItemFreeCB)(void *resource, void *udata);

/**
 * @typedef CMPoolItemTestCB Callback type for testing a pool item.
 */
typedef CMBool (*CMPoolItemTestCB)(void *resource, void *udata);

/**
 * @brief Create a resource pool with specified parameters.
 *
 * @param initcnt The initial number of resources in the pool.
 * @param maxcnt The maximum number of resources allowed in the pool.
 * @param createproc A callback function to create new resources.
 * @param destroyproc A callback function to free resources.
 * @param testproc A callback function to test the validity of resources.
 * @param pinginterval The interval in milliseconds to ping resources for validity.
 * @param testonborrow Whether to test resources when they are checked out.
 * @param udata User-defined data to be passed to callback functions.
 * @param timer A timer object for scheduling resource pings.
 *         Ownership of the timer object is not transferred to the pool.
 *         If NULL, creates an internal default timer.
 * @return A pointer to the newly created resource pool,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_Pool *CMUTIL_PoolCreate(
        int initcnt,
        int maxcnt,
        CMPoolItemCreateCB createproc,
        CMPoolItemFreeCB destroyproc,
        CMPoolItemTestCB testproc,
        long pinginterval,
        CMBool testonborrow,
        void *udata,
        CMUTIL_Timer *timer);



/**
 * @typedef CMUTIL_Library Dynamic library loader.
 */
typedef struct CMUTIL_Library CMUTIL_Library;
struct CMUTIL_Library {

    /**
     * @brief Get a procedure (function) from the dynamic library.
     *
     * This function retrieves a pointer to a function
     * with the specified name from the loaded dynamic library.
     *
     * @param lib A pointer to the CMUTIL_Library object representing
     *            the loaded dynamic library.
     * @param proc_name The name of the procedure (function) to retrieve.
     * @return A pointer to the requested procedure,
     *         or NULL if the procedure was not found.
     */
    void *(*GetProcedure)(
            const CMUTIL_Library *lib,
            const char *proc_name);

    /**
     * @brief Destroy the dynamic library loader and unload the library.
     *
     * This function unloads the dynamic library associated with
     * the CMUTIL_Library object and frees any associated resources.
     * After calling this function, the CMUTIL_Library object should
     * not be used again.
     *
     * @param lib A pointer to the CMUTIL_Library object to be destroyed.
     */
    void (*Destroy)(
            CMUTIL_Library *lib);
};

/**
 * @brief Create a dynamic library loader for the specified library path.
 *
 * This function loads a dynamic library from the specified file path
 * and returns a CMUTIL_Library object that can be used to access
 * functions within the library.
 *
 * @param path The file path to the dynamic library to load.
 *             The file extension can be excluded.
 * @return A pointer to the newly created CMUTIL_Library object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_Library *CMUTIL_LibraryCreate(const char *path);


/**
 * @typedef CMUTIL_FileList A list of files in a directory.
 */
typedef struct CMUTIL_FileList CMUTIL_FileList;

/**
 * @typedef CMUTIL_File A file or directory object.
 */
typedef struct CMUTIL_File CMUTIL_File;
struct CMUTIL_FileList {
    /**
     * @brief Get the number of files in the list.
     *
     * @param flist A pointer to the CMUTIL_FileList object.
     * @return The number of files in the list.
     */
    size_t (*Count)(
            const CMUTIL_FileList *flist);

    /**
     * @brief Get the file at the specified index in the list.
     *
     * @param flist A pointer to the CMUTIL_FileList object.
     * @param index The index of the file to retrieve.
     * @return A pointer to the CMUTIL_File object at the specified index,
     *         or NULL if the index is out of bounds.
     */
    CMUTIL_File *(*GetAt)(
            const CMUTIL_FileList *flist,
            uint32_t index);

    /**
     * @brief Destroy the file list and free its resources.
     *
     * @param flist A pointer to the CMUTIL_FileList object to be destroyed.
     */
    void (*Destroy)(
            CMUTIL_FileList *flist);
};

/**
 * @typedef CMUTIL_FileOpenMode File open mode enumeration.
 */
typedef enum CMFileOpenMode {
    /** Open file for reading. */
    CMFileOpenRead = 0,
    /** Open file for writing (overwrite). */
    CMFileOpenWrite,
    /** Open file for appending. */
    CMFileOpenAppend
} CMFileOpenMode;

/**
 * @typedef CMUTIL_FileStream File stream object for reading and writing files.
 */
typedef struct CMUTIL_FileStream CMUTIL_FileStream;
struct CMUTIL_FileStream {
        /**
         * @brief Read data from the file stream into a buffer.
         *
         * This method only available if this object created with
         * CMFileOpenRead mode, otherwise will return -1.
         *
         * @param fs A pointer to the CMUTIL_FileStream object.
         * @param buffer A pointer to the buffer where the read data will be stored.
         * @param size The number of bytes to read from the file stream.
         * @return The number of bytes actually read, or -1 on failure.
         */
        ssize_t (*Read)(
                const CMUTIL_FileStream *fs,
                CMUTIL_String *buffer,
                size_t size);

        /**
         * @brief Write data from a buffer to the file stream.
         *
         * This method only available if this object created with
         * CMFileOpenWrite or CMFileOpenAppend mode, otherwise will return -1.
         *
         * @param fs A pointer to the CMUTIL_FileStream object.
         * @param buffer A pointer to the buffer containing the data to write.
         * @param offset The offset in the buffer from which to start writing.
         * @param size The number of bytes to write to the file stream.
         * @return The number of bytes actually written, or -1 on failure.
         */
        ssize_t (*Write)(
                const CMUTIL_FileStream *fs,
                const CMUTIL_String *buffer,
                size_t offset,
                size_t size);

        /**
         * @brief Close the file stream and free its resources.
         *
         * @param fs A pointer to the CMUTIL_FileStream object to be closed.
         */
        void (*Close)(
                CMUTIL_FileStream *fs);
};

struct CMUTIL_File {
    /**
     * @brief Get the contents of the file as a string.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return A pointer to a CMUTIL_String object containing the file contents,
     *         or NULL on failure.
     */
    CMUTIL_String *(*GetContents)(
            const CMUTIL_File *file);

    /**
     * @brief Delete the file.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return CMTrue on success, CMFalse on failure.
     */
    CMBool (*Delete)(
            const CMUTIL_File *file);

    /**
     * @brief Check if the file is a regular file.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return CMTrue if the file is a regular file, CMFalse otherwise.
     */
    CMBool (*IsFile)(
            const CMUTIL_File *file);

    /**
     * @brief Check if the file is a directory.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return CMTrue if the file is a directory, CMFalse otherwise.
     */
    CMBool (*IsDirectory)(
            const CMUTIL_File *file);

    /**
     * @brief Check if the file exists.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return CMTrue if the file exists, CMFalse otherwise.
     */
    CMBool (*IsExists)(
            const CMUTIL_File *file);

    /**
     * @brief Get the length of the file in bytes.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return The length of the file in bytes, or -1 on failure.
     */
    long (*Length)(
            const CMUTIL_File *file);

    /**
     * @brief Get the name of the file or directory.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return A pointer to a C-style string containing the name of the file
     *         or directory.
     */
    const char *(*GetName)(
            const CMUTIL_File *file);

    /**
     * @brief Get the full path of the file or directory.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return A pointer to a C-style string containing the full path of the file
     *         or directory.
     */
    const char *(*GetFullPath)(
            const CMUTIL_File *file);

    /**
     * @brief Get the last modified time of the file.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @return The last modified time of the file as a time_t value.
     */
    time_t (*ModifiedTime)(
            const CMUTIL_File *file);

    /**
     * @brief Get a list of all children files and directories.
     *
     * @param file A pointer to the CMUTIL_File object representing a directory.
     * @return A pointer to a CMUTIL_FileList object containing all children
     *         files and directories. The caller is responsible for freeing the
     *         returned object.
     */
    CMUTIL_FileList *(*Children)(
            const CMUTIL_File *file);

    /**
     * @brief Find all children whose name is matched with the given pattern.
     *
     *  Filename patterns are composed of regular (printable) characters which
     *  may comprise a filename as well as special pattern matching characters:
     *
     * <ul>
     *      <li>.     - Matches a period (.).</li>
     *          Note that a period in a filename is not treated any
     *          differently than any other character.
     *
     *      <li>?     - Any.</li>
     *          Matches any single character except '/' or '\'.
     *
     *      <li>*     - Closure.</li>
     *          Matches zero or more occurences of any characters other
     *          than '/' or '\'.
     *          Leading '*' characters are allowed.
     *
     *      <li>SUB   - Substitute (^Z).</li>
     *          Similar to '*', this matches zero or more occurences of
     *          any characters other than '/', '\', or '.'.
     *          Leading '^Z' characters are allowed.
     *
     *      <li>[ab]  - Set.</li>
     *          Matches the single character 'a' or 'b'.
     *          If the dash '-' character is to be included, it must
     *          immediately follow the opening bracket '['.
     *          If the closing bracket ']' character is to be included,
     *          it must be preceded by a quote '`'.
     *
     *      <li>[a-z] - Range.</li>
     *          Matches a single character in the range 'a' to 'z'.
     *          Ranges and sets may be combined within the same set of
     *          brackets.
     *
     *      <li>[!R]  - Exclusive range.</li>
     *          Matches a single character not in the range 'R'.
     *          If range 'R' includes the dash '-' character, the dash
     *          must immediately follow the caret '!'.
     *
     *      <li>!     - Not.</li>
     *          Makes the following pattern (up to the next '/') match
     *          any filename except those what it would normally match.
     *
     *      <li>/     - Path separator (UNIX and DOS).</li>
     *          Matches a '/' or '\' pathname (directory) separator.
     *          Multiple separators are treated like a single
     *          separator.
     *          A leading separator indicates an absolute pathname.
     *
     *      <li>\     - Path separator (DOS).</li>
     *          Same as the '/' character.
     *          Note that this character must be escaped if used within
     *          string constants ("\\").
     *
     *      <li>\     - Quote (UNIX).</li>
     *          Makes the next character a regular (nonspecial)
     *          character.
     *          Note that to match the quote character itself, it must
     *          be quoted.
     *          Note that this character must be escaped if used within
     *          string constants ("\\").
     *
     *      <li>`     - Quote (DOS).</li>
     *          Makes the next character a regular (nonspecial)
     *          character.
     *          Note that to match the quote character itself, it must
     *          be quoted.
     * </ul>
     *
     *  Upper and lower case alphabetic characters are considered identical,
     *  i.e., 'a' and 'A' match each other.
     *  (What constitutes a lowercase letter depends on the current locale
     *  settings.)
     *
     *  Spaces and control characters are treated as normal characters.
     * <br/>
     * <b>Examples</b><br/>
     *  The following patterns in the left column will match the filenames in
     *  the middle column and will not match filenames in the right column:
     *
     * <pre>
     *      Pattern     Will Match                      Will Not Match
     *      -------     ----------                      --------------
     *      a           a (only)                        (anything else)
     *      a.          a. (only)                       (anything else)
     *      a?c         abc, acc, arc, a.c              a, ac, abbc
     *      a*c         ac, abc, abbc, acc, a.c         a, ab, acb, bac
     *      a*          a, ab, abb, a., a.b             b, ba
     *      *           a, ab, abb, a., .foo, a.foo     (nothing)
     *      *.          a., ab., abb., a.foo.           a, ab, a.foo, .foo
     *      *.*         a., a.b, ah.bc.foo              a
     *      ^Z          a, ab, abb                      a., .foo, a.foo
     *      ^Z.         a., ab., abb.                   a, .foo, a.foo
     *      ^Z.*        a, a., .foo, a.foo              ab, abb
     *      *2.c        2.c, 12.c, foo2.c, foo.12.c     2x.c
     *      a[b-z]c     abc, acc, azc (only)            (anything else)
     *      [ab0-9]x    ax, bx, 0x, 9x                  zx
     *      a[-.]b      a-b, a.b (only)                 (anything else)
     *      a[!a-z]b    a0b, a.b, a@b                   aab, azb, aa0b
     *      a[!-b]x     a0x, a+x, acx                   a-x, abx, axxx
     *      a[-!b]x     a-x, a!x, abx (only)            (anything else)
     *      a[`]]x      a]x (only)                      (anything else)
     *      a``x        a`x (only)                      (anything else)
     *      oh`!        oh! (only)                      (anything else)
     *      is`?it      is?it (only)                    (anything else)
     *      !a?c        a, ac, ab, abb, acb, a.foo      abc, a.c, azc
     * </pre>
     *
     * @param file A pointer to the CMUTIL_File object representing a directory.
     * @param pattern The filename pattern to match.
     * @param recursive Whether to search recursively in subdirectories.
     * @return A pointer to a CMUTIL_FileList object containing all matched
     *         files and directories.
     */
    CMUTIL_FileList *(*Find)(
            const CMUTIL_File *file,
            const char *pattern, CMBool recursive);

    /**
     * @brief Create a file stream for reading or writing the file.
     *
     * @param file A pointer to the CMUTIL_File object.
     * @param mode The file open mode (read, write, append).
     * @return A pointer to the newly created CMUTIL_FileStream object,
     *         or NULL on failure.
     */
    CMUTIL_FileStream *(*CreateStream)(
            const CMUTIL_File *file,
            CMFileOpenMode mode);

    /**
     * @brief Destroy the file or directory object and free its resources.
     *
     * @param file A pointer to the CMUTIL_File object to be destroyed.
     */
    void (*Destroy)(
            CMUTIL_File *file);
};

/**
 * @brief Create a file or directory object for the specified path.
 *
 * @param path The file or directory path.
 * @return A pointer to the newly created CMUTIL_File object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_File *CMUTIL_FileCreate(const char *path);

/**
 * @brief Create directories recursively for the specified path.
 *
 * This function creates all necessary parent directories
 * for the given path if they do not already exist.
 *
 * @param path The directory path to create.
 * @param mode The permissions to set for newly created directories.
 * @return CMTrue on success, CMFalse on failure.
 */
CMUTIL_API CMBool CMUTIL_PathCreate(const char *path, uint32_t mode);

/**
 * @typedef CMUTIL_Config Configuration management object.
 */
typedef struct CMUTIL_Config CMUTIL_Config;
struct CMUTIL_Config {
    /**
     * @brief Save the configuration to a file.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param path The file path where the configuration will be saved.
     */
    void (*Save)(const CMUTIL_Config *conf, const char *path);

    /**
     * @brief Get a configuration value as a string.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @return The configuration value as a C-style string,
     *         or NULL if the key does not exist.
     */
    const char *(*Get)(const CMUTIL_Config *conf, const char *key);

    /**
     * @brief Set a configuration value as a string.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @param value The configuration value to set.
     */
    void (*Set)(CMUTIL_Config *conf, const char *key, const char *value);

    /**
     * @brief Get a configuration value as a long integer.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @return The configuration value as a long integer.
     */
    long (*GetLong)(const CMUTIL_Config *conf, const char *key);

    /**
     * @brief Set a configuration value as a long integer.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @param value The configuration value to set.
     */
    void (*SetLong)(CMUTIL_Config *conf, const char *key, long value);

    /**
     * @brief Get a configuration value as a double.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @return The configuration value as a double.
     */
    double (*GetDouble)(const CMUTIL_Config *conf, const char *key);

    /**
     * @brief Set a configuration value as a double.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @param value The configuration value to set.
     */
    void (*SetDouble)(CMUTIL_Config *conf, const char *key, double value);

    /**
     * @brief Get a configuration value as a boolean.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @return The configuration value as a boolean.
     */
    CMBool (*GetBoolean)(const CMUTIL_Config *conf, const char *key);

    /**
     * @brief Set a configuration value as a boolean.
     *
     * @param conf A pointer to the CMUTIL_Config object.
     * @param key The configuration key.
     * @param value The configuration value to set.
     */
    void (*SetBoolean)(CMUTIL_Config *conf, const char *key, CMBool value);

    /**
     * @brief Destroy the configuration object and free its resources.
     *
     * @param conf A pointer to the CMUTIL_Config object to be destroyed.
     */
    void(*Destroy)(CMUTIL_Config *conf);
};

/**
 * @brief Create a new empty configuration object.
 *
 * @return A pointer to the newly created CMUTIL_Config object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_Config *CMUTIL_ConfigCreate(void);

/**
 * @brief Load a configuration from a file.
 *
 * @param fconf The file path of the configuration file to load.
 * @return A pointer to the loaded CMUTIL_Config object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_Config *CMUTIL_ConfigLoad(const char *fconf);


/**
 * Default log configuration file name.
 */
#define CMUTIL_LOG_CONFIG_DEFAULT       "cmutil_log.jsonc"

/**
 * @typedef CMLogLevel Log severity levels.
 */
typedef enum CMLogLevel {
    /** Trace level for detailed debugging information. */
    CMLogLevel_Trace = 0,
    /** Debug level for general debugging information. */
    CMLogLevel_Debug,
    /** Info level for informational messages. */
    CMLogLevel_Info,
    /** Warn level for warning messages. */
    CMLogLevel_Warn,
    /** Error level for error messages. */
    CMLogLevel_Error,
    /** Fatal level for critical error messages. */
    CMLogLevel_Fatal
} CMLogLevel;

/**
 * @typedef CMLogTerm Log rolling terms.
 */
typedef enum CMLogTerm {
    /** Yearly log rolling. */
    CMLogTerm_Year = 0,
    /** Monthly log rolling. */
    CMLogTerm_Month,
    /** Daily log rolling. */
    CMLogTerm_Date,
    /** Hourly log rolling. */
    CMLogTerm_Hour,
    /** Minutely log rolling. */
    CMLogTerm_Minute
} CMLogTerm;

/**
 * @typedef CMUTIL_LogAppender Log appender object.
 */
typedef struct CMUTIL_LogAppender CMUTIL_LogAppender;

/**
 * @typedef CMUTIL_ConfLogger Logger configuration object.
 */
typedef struct CMUTIL_ConfLogger CMUTIL_ConfLogger;
struct CMUTIL_ConfLogger {

    /**
     * @brief Add a log appender to the logger configuration.
     *
     * This function adds a log appender to the logger configuration
     * with the specified log level.
     *
     * @param logger A pointer to the CMUTIL_ConfLogger object.
     * @param appender A pointer to the CMUTIL_LogAppender object to add.
     * @param level The log level for the appender.
     */
    void(*AddAppender)(
        const CMUTIL_ConfLogger *logger,
        CMUTIL_LogAppender *appender,
        CMLogLevel level);
};

/**
 * @typedef CMUTIL_Logger Logger object.
 */
typedef struct CMUTIL_Logger CMUTIL_Logger;
struct CMUTIL_Logger {
    /**
     * @brief Log a message with extended information.
     *
     * This function logs a message at the specified log level,
     * including the source file name, line number, and optionally
     * a stack trace.
     *
     * @param logger A pointer to the CMUTIL_Logger object.
     * @param level The log level for the message.
     * @param file The source file name where the log is generated.
     * @param line The line number in the source file.
     * @param printStack A boolean indicating whether to include a stack trace.
     * @param fmt The format string for the log message.
     * @param ... Additional arguments for the format string.
     */
    void(*LogEx)(
        CMUTIL_Logger *logger,
        CMLogLevel level,
        const char *file,
        int line,
        CMBool printStack,
        const char *fmt,
        ...);

    /**
     * @brief Check if logging is enabled for the specified log level.
     *
     * This function checks whether logging is enabled for the given
     * log level in the logger.
     *
     * @param logger A pointer to the CMUTIL_Logger object.
     * @param level The log level to check.
     * @return CMTrue if logging is enabled for the specified level,
     *         CMFalse otherwise.
     */
    CMBool(*IsEnabled)(
            CMUTIL_Logger *logger,
            CMLogLevel level);
};

struct CMUTIL_LogAppender {
    /**
     * @brief Get the name of the log appender.
     *
     * This function retrieves the name of the log appender.
     *
     * @param appender A pointer to the CMUTIL_LogAppender object.
     * @return A pointer to a C-style string containing the name of the appender.
     */
    const char *(*GetName)(
            const CMUTIL_LogAppender *appender);

    /**
     * @brief Set the log appender to operate in asynchronous mode.
     *
     * This function configures the log appender to use asynchronous
     * logging with the specified buffer size.
     *
     * @param appender A pointer to the CMUTIL_LogAppender object.
     * @param buffersz The size of the buffer for asynchronous logging.
     */
    void(*SetAsync)(
            CMUTIL_LogAppender *appender, size_t buffersz);

    /**
     * @brief Append a log message to the appender.
     *
     * This function appends a log message to the log appender
     * with the specified log level, stack trace, source file,
     * line number, and log message.
     *
     * @param appender A pointer to the CMUTIL_LogAppender object.
     * @param logger A pointer to the CMUTIL_Logger object.
     * @param level The log level for the message.
     * @param stack A pointer to a CMUTIL_StringArray containing the stack trace.
     * @param file The source file name where the log is generated.
     * @param line The line number in the source file.
     * @param logmsg A pointer to a CMUTIL_String containing the log message.
     */
    void(*Append)(
        CMUTIL_LogAppender *appender,
        CMUTIL_Logger *logger,
        CMLogLevel level,
        CMUTIL_StringArray *stack,
        const char *file,
        int line,
        CMUTIL_String *logmsg);

    /**
     * @brief Flush any buffered log messages.
     *
     * This function flushes any buffered log messages in the log appender
     * to their final destination (e.g., console, file, socket).
     *
     * @param appender A pointer to the CMUTIL_LogAppender object.
     */
    void(*Flush)(CMUTIL_LogAppender *appender);

    /**
     * @brief Destroy the log appender and free its resources.
     *
     * This function releases all resources associated with the log appender
     * and frees any allocated memory.
     *
     * @param appender A pointer to the CMUTIL_LogAppender object to be destroyed.
     */
    void(*Destroy)(CMUTIL_LogAppender *appender);
};

/**
 * @brief Create a console log appender.
 *
 * @param name The name of the log appender.
 * @param pattern The log message pattern.
 * @param use_stderr Whether to use stderr instead of stdout for logging.
 * @return A pointer to the newly created CMUTIL_LogAppender object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreate(
    const char *name,
    const char *pattern,
    CMBool use_stderr);

/**
 * @brief Create a file log appender.
 *
 * @param name The name of the log appender.
 * @param fpath The file path for the log file.
 * @param pattern The log message pattern.
 * @return A pointer to the newly created CMUTIL_LogAppender object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreate(
    const char *name,
    const char *fpath,
    const char *pattern);

/**
 * @brief Create a rolling file log appender.
 *
 * @param name The name of the log appender.
 * @param fpath The file path for the log file.
 * @param logterm The log rolling term.
 * @param rollpath The roll file path pattern.
 * @param pattern The log message pattern.
 * @return A pointer to the newly created CMUTIL_LogAppender object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreate(
    const char *name,
    const char *fpath,
    CMLogTerm logterm,
    const char *rollpath,
    const char *pattern);

/**
 * @brief Create a socket log appender.
 *
 * @param name The name of the log appender.
 * @param accept_host Listen host address.(0.0.0.0 for any address)
 * @param listen_port Listen port number.
 * @param pattern The log message pattern.
 * @return A pointer to the newly created CMUTIL_LogAppender object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreate(
    const char *name,
    const char *accept_host,
    int listen_port,
    const char *pattern);

/**
 * @brief Fallback logging function when no logger is available.
 *
 * @param level Log level.
 * @param file Source file name.
 * @param line Line number in source file.
 * @param fmt Format string for the log message.
 * @param ... Additional arguments for the format string.
 */
CMUTIL_API void CMUTIL_LogFallback(CMLogLevel level,
        const char *file, int line, const char *fmt, ...);

/**
 * @brief Define a logger for the current source file.
 *
 * This macro defines a static logger instance for the current source file
 * with the specified name. It provides a function to retrieve the logger
 * instance, creating it if it does not already exist or if the log system
 * has changed.
 *
 * @param name The name of the logger.
 */
#define CMUTIL_LogDefine(name)                                          \
    static CMUTIL_Logger *___logger = NULL;                             \
    static CMUTIL_LogSystem *__lsys = NULL;                             \
    static CMUTIL_Logger *__CMUTIL_GetLogger() {                        \
        if (___logger == NULL || __lsys != CMUTIL_LogSystemGet()) {     \
            __lsys = CMUTIL_LogSystemGet();                             \
            if (__lsys) ___logger = CMCall(__lsys, GetLogger, name);    \
            else ___logger = NULL;                                      \
        }                                                               \
        return ___logger;                                               \
    }

/**
 * @brief Check if logging is enabled for the specified log level.
 *
 * This function checks whether logging is enabled for the given
 * log level in the specified logger.
 *
 * @param logger A pointer to the CMUTIL_Logger object.
 * @param level The log level to check.
 * @return CMTrue if logging is enabled for the specified level,
 *         CMFalse otherwise.
 */
CMUTIL_API CMBool CMUTIL_LogIsEnabled(
        CMUTIL_Logger *logger, CMLogLevel level);

/**
 * @brief Macro to check if logging is enabled for the specified log level.
 *
 * This macro checks whether logging is enabled for the given
 * log level in the default logger.
 *
 * @param level The log level to check.
 * @return CMTrue if logging is enabled for the specified level,
 *         CMFalse otherwise.
 */
#define CMLogIsEnabled(level) CMUTIL_LogIsEnabled(__CMUTIL_GetLogger(),level)

/**
 * @brief Log a message with the specified log level and stack trace option.
 *
 * This macro logs a message at the specified log level, including
 * the source file name, line number, and optionally a stack trace.
 *
 * @param level The simple log level for the message.
 * @param stack A boolean indicating whether to include a stack trace.
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMUTIL_Log__(level,stack,f,...) do {                            \
    CMUTIL_Logger *__logger = __CMUTIL_GetLogger();                     \
    if (__logger)                                                       \
        __logger->LogEx(__logger, CMLogLevel_##level,__FILE__,__LINE__, \
                      CM##stack,f,##__VA_ARGS__);                       \
    else                                                                \
        CMUTIL_LogFallback(CMLogLevel_##level, __FILE__, __LINE__,      \
                           f, ##__VA_ARGS__);                           \
    } while(0)

/**
 * @brief Log a message with the specified log level and stack trace option.
 *
 * This macro logs a message at the specified log level, including
 * the source file name, line number, and optionally a stack trace.
 *
 * @param level The full log level name for the message.
 * @param stack A boolean indicating whether to include a stack trace.
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMUTIL_Log2__(level,stack,f,...)    do {                        \
    CMUTIL_Logger *__logger = __CMUTIL_GetLogger();                     \
    if (__logger)                                                       \
        __logger->LogEx(__logger, level,__FILE__,__LINE__,              \
                      CM##stack,f,##__VA_ARGS__);                       \
    else                                                                \
        CMUTIL_LogFallback(CMLogLevel_##level, __FILE__, __LINE__,      \
                           f, ##__VA_ARGS__);                           \
    } while(0)

/**
 * @brief Log a message at the Trace level.
 *
 * This macro logs a message at the Trace level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogTrace(f,...)   CMUTIL_Log__(Trace,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Trace level with stack trace.
 *
 * This macro logs a message at the Trace level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogTraceS(f,...)  CMUTIL_Log__(Trace,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Debug level.
 *
 * This macro logs a message at the Debug level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogDebug(f,...)   CMUTIL_Log__(Debug,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Debug level with stack trace.
 *
 * This macro logs a message at the Debug level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogDebugS(f,...)  CMUTIL_Log__(Debug,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Info level.
 *
 * This macro logs a message at the Info level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogInfo(f,...)    CMUTIL_Log__(Info ,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Info level with stack trace.
 *
 * This macro logs a message at the Info level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogInfoS(f,...)   CMUTIL_Log__(Info ,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Warn level.
 *
 * This macro logs a message at the Warn level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogWarn(f,...)    CMUTIL_Log__(Warn ,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Warn level with stack trace.
 *
 * This macro logs a message at the Warn level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogWarnS(f,...)   CMUTIL_Log__(Warn ,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Error level.
 *
 * This macro logs a message at the Error level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogError(f,...)   CMUTIL_Log__(Error,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Error level with stack trace.
 *
 * This macro logs a message at the Error level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogErrorS(f,...)  CMUTIL_Log__(Error,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Fatal level.
 *
 * This macro logs a message at the Fatal level, including
 * the source file name and line number.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogFatal(f,...)   CMUTIL_Log__(Fatal,False,f,##__VA_ARGS__)

/**
 * @brief Log a message at the Fatal level with stack trace.
 *
 * This macro logs a message at the Fatal level, including
 * the source file name, line number, and a stack trace.
 *
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogFatalS(f,...)  CMUTIL_Log__(Fatal,True ,f,##__VA_ARGS__)

/**
 * @brief Log a message with the specified log level.
 *
 * This macro logs a message at the specified log level, including
 * the source file name, line number.
 *
 * @param l The full log level for the message.
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLog(l,f,...)      CMUTIL_Log2__(l,False,f,##__VA_ARGS__)

/**
 * @brief Log a message with the specified log level and stack trace.
 *
 * This macro logs a message at the specified log level, including
 * the source file name, line number, and stack trace.
 *
 * @param l The full log level for the message.
 * @param f The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define CMLogS(l,f,...)     CMUTIL_Log2__(l,True ,f,##__VA_ARGS__)

/**
 * @typedef CMUTIL_LogSystem Log system object.
 */
typedef struct CMUTIL_LogSystem CMUTIL_LogSystem;
struct CMUTIL_LogSystem {

    /**
     * @brief Add a log appender to the log system.
     *
     * This method will add a log appender to the log system.
     *
     * @param logsys The log system object.
     * @param appender The log appender to be added.
     */
    void(*AddAppender)(
            const CMUTIL_LogSystem *logsys, CMUTIL_LogAppender *appender);
    /**
     * @brief Create a new logger object.
     *
     * This method will create a new logger object with the given name,
     * log level, and additivity.
     *
     * @param logsys The log system object.
     * @param name The name of logger. If NULL or empty string is given, a
     *          root logger will be created.
     * @param level The log level of this logger.
     * @param additivity If CMTrue, this logger will also log to parent
     *          loggers' appenders.
     * @return The created logger object.
     */
    CMUTIL_ConfLogger *(*CreateLogger)(
        const CMUTIL_LogSystem *logsys,
        const char *name,
        CMLogLevel level,
        CMBool additivity);

    /**
     * @brief Get a logger by name.
     *
     * This method retrieves a logger object by its name from the log system.
     *
     * @param logsys The log system object.
     * @param name The name of the logger to retrieve.
     * @return The logger object with the specified name, or NULL if not found.
     */
    CMUTIL_Logger *(*GetLogger)(
            const CMUTIL_LogSystem *logsys, const char *name);

    /**
     * @brief Destroy the log system and free its resources.
     *
     * This method releases all resources associated with the log system
     * and frees any allocated memory.
     *
     * @param logsys A pointer to the CMUTIL_LogSystem object to be destroyed.
     */
    void(*Destroy)(CMUTIL_LogSystem *logsys);
};

/**
 * @brief Create a new log system object.
 *
 * @return A pointer to the newly created CMUTIL_LogSystem object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemCreate(void);

/**
 * @brief Configure the log system from a JSON configuration file.
 *
 * @param jsonfile The path to the JSON configuration file.
 * @return A pointer to the configured CMUTIL_LogSystem object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(
        const char *jsonfile);

/**
 * @brief Get the current global log system.
 *
 * @return A pointer to the current CMUTIL_LogSystem object,
 *         or NULL if no log system is set.
 */
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemGet(void);

/**
 * @brief Set the global log system.
 *
 * @param lsys A pointer to the CMUTIL_LogSystem object to set as the global log system.
 */
CMUTIL_API void CMUTIL_LogSystemSet(CMUTIL_LogSystem *lsys);

/**
 * @typedef CMUTIL_StackWalker Stack walker object.
 */
typedef struct CMUTIL_StackWalker CMUTIL_StackWalker;
struct CMUTIL_StackWalker {

    /**
     * @brief Get the current call stack as a string array.
     *
     * This method retrieves the current call stack, skipping
     * a specified number of frames from the top.
     *
     * @param walker The stack walker object.
     * @param skipdepth The number of frames to skip from the top of the stack.
     * @return A pointer to a CMUTIL_StringArray containing the call stack.
     */
    CMUTIL_StringArray *(*GetStack)(
            const CMUTIL_StackWalker *walker, int skipdepth);

    /**
     * @brief Print the current call stack to a string buffer.
     *
     * This method prints the current call stack into the provided
     * string buffer, skipping a specified number of frames from the top.
     *
     * @param walker The stack walker object.
     * @param outbuf The string buffer to store the printed stack trace.
     * @param skipdepth The number of frames to skip from the top of the stack.
     */
    void(*PrintStack)(
            const CMUTIL_StackWalker *walker,
            CMUTIL_String *outbuf, int skipdepth);

    /**
     * @brief Destroy the stack walker and free its resources.
     *
     * This method releases all resources associated with the stack walker
     * and frees any allocated memory.
     *
     * @param walker A pointer to the CMUTIL_StackWalker object to be destroyed.
     */
    void(*Destroy)(CMUTIL_StackWalker *walker);
};

/**
 * @brief Create a new stack walker object.
 *
 * @return A pointer to the newly created CMUTIL_StackWalker object,
 *         or NULL on failure.
 */
CMUTIL_API CMUTIL_StackWalker *CMUTIL_StackWalkerCreate(void);


/**
 * @typedef CMSocketResult Socket operation result codes.
 */
typedef enum CMSocketResult {
    /** Operation succeeded. */
    CMSocketOk = 0,
    /** Operation timed out. */
    CMSocketTimeout,
    /** Polling the socket failed. */
    CMSocketPollFailed,
    /** Receiving data from the socket failed. */
    CMSocketReceiveFailed,
    /** Sending data to the socket failed. */
    CMSocketSendFailed,
    /** The socket operation is unsupported. */
    CMSocketUnsupported,
    /** The socket is not connected. */
    CMSocketNotConnected,
    /** Connecting the socket failed. */
    CMSocketConnectFailed,
    /** Binding the socket failed. */
    CMSocketBindFailed,
    /** Unknown error occurred. */
    CMSocketUnknownError = 0x7FFFFFFF
} CMSocketResult;

/**
 * @typedef CMUTIL_SocketAddr Socket address object.
 */
typedef struct sockaddr_in CMUTIL_SocketAddr;

/**
 * @brief Get the host and port from a socket address.
 *
 * @param saddr Socket address object.
 * @param hostbuf Buffer to store the host string. If NULL, host is not retrieved.
 * @param port Pointer to store the port number. If NULL, port is not retrieved.
 * @return CMSocketResult indicating success or failure.
 */
CMUTIL_API CMSocketResult CMUTIL_SocketAddrGet(
        const CMUTIL_SocketAddr *saddr, char *hostbuf, int *port);

/**
 * @brief Set the host and port in a socket address.
 *
 * @param saddr Socket address object to be set.
 * @param host Host string to set.
 * @param port Port number to set.
 * @return CMSocketResult indicating success or failure.
 */
CMUTIL_API CMSocketResult CMUTIL_SocketAddrSet(
        CMUTIL_SocketAddr *saddr, const char *host, int port);

/**
 * @typedef CMUTIL_Socket Socket object.
 */
typedef struct CMUTIL_Socket CMUTIL_Socket;
struct CMUTIL_Socket {

    /**
     * @brief Read data from the socket into a buffer.
     *
     * Reads up to 'size' bytes of data from the specified socket
     * into the provided byte buffer within the given timeout period.
     *
     * @param socket The socket object to read from.
     * @param buffer The byte buffer to store the read data.
     * @param size The maximum number of bytes to read.
     * @param timeout The read operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Read)(
            const CMUTIL_Socket *socket,
            CMUTIL_ByteBuffer *buffer,
            uint32_t size, long timeout);

    /**
     * @brief Write the given data to the socket into a buffer.
     *
     * Writes all data from the provided byte buffer to the specified
     * socket within the specified timeout period.
     *
     * @param socket The socket object to write to.
     * @param data The data to write.
     * @param size The size of the data to write.
     * @param timeout The read operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Write)(
            const CMUTIL_Socket *socket,
            const void *data,
            uint32_t size, long timeout);

    /**
     * @brief Write a specific part of data to the socket from a buffer.
     *
     * Writes 'length' bytes of data from the provided string buffer
     * starting at the given offset to the specified socket within
     * the specified timeout period.
     *
     * @param socket The socket object to write to.
     * @param data The data to write.
     * @param offset The offset in the buffer to start writing from.
     * @param length The number of bytes to write.
     * @param timeout The write operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*WritePart)(
            const CMUTIL_Socket *socket, const void *data,
            uint32_t offset, uint32_t length, long timeout);

    /**
     * @brief Check if the socket is ready for reading.
     *
     * This function checks if there is data available to read from the socket
     * within the specified timeout period.
     *
     * @param socket The socket object to check.
     * @param timeout The timeout period in milliseconds.
     * @return CMSocketResult indicating if the socket is ready for reading.
     */
    CMSocketResult (*CheckReadBuffer)(
            const CMUTIL_Socket *socket, long timeout);

    /**
     * @brief Check if the socket is ready for writing.
     *
     * This function checks if the socket is ready to accept data for writing
     * within the specified timeout period.
     *
     * @param socket The socket object to check.
     * @param timeout The timeout period in milliseconds.
     * @return CMSocketResult indicating if the socket is ready for writing.
     */
    CMSocketResult (*CheckWriteBuffer)(
            const CMUTIL_Socket *socket, long timeout);

    /**
     * @brief Read a socket object from the current socket.
     *
     * This function reads a new socket object from the current socket
     * within the specified timeout period.
     *
     * @param socket The socket object to read from.
     * @param timeout The read operation timeout in milliseconds.
     * @param rval Pointer to store the result of the read operation.
     * @return A new CMUTIL_Socket object if successful, NULL otherwise.
     */
    CMUTIL_Socket *(*ReadSocket)(
            const CMUTIL_Socket *socket,
            long timeout, CMSocketResult *rval);

    /**
     * @brief Write a socket object to the current socket.
     *
     * This function writes the specified socket object to the current socket
     * within the specified timeout period.
     *
     * @param socket The socket object to write to.
     * @param tobesent The socket object to be sent.
     * @param pid The process ID associated with the socket.
     * @param timeout The write operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*WriteSocket)(
            const CMUTIL_Socket *socket,
            CMUTIL_Socket *tobesent, pid_t pid, long timeout);

    /**
     * @brief Get the remote address of the socket.
     *
     * This function retrieves the remote address of the specified socket
     * and stores it in the provided socket address object.
     *
     * @param socket The socket object to get the remote address from.
     * @param addr Pointer to the CMUTIL_SocketAddr object to store the address.
     */
    void (*GetRemoteAddr)(
            const CMUTIL_Socket *socket, CMUTIL_SocketAddr *addr);

    /**
     * @brief Close the socket.
     *
     * This function closes the specified socket and releases any associated resources.
     *
     * @param socket The socket object to be closed.
     */
    void (*Close)(
            CMUTIL_Socket *socket);

    /**
     * @brief Get the raw socket descriptor.
     *
     * This function retrieves the underlying raw socket descriptor
     * associated with the specified socket object.
     *
     * @param socket The socket object to get the raw socket from.
     * @return The raw socket descriptor.
     */
    SOCKET (*GetRawSocket)(
            const CMUTIL_Socket *socket);

    /**
     * @brief Read a single byte from the socket.
     *
     * This function reads a single byte from the specified socket.
     *
     * @param socket The socket object to read from.
     * @return The byte read from the socket, or -1 on failure.
     */
    int (*ReadByte)(
            const CMUTIL_Socket *socket);

    /**
     * @brief Write a single byte to the socket.
     *
     * This function writes a single byte to the specified socket.
     *
     * @param socket The socket object to write to.
     * @param c The byte to be written.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*WriteByte)(
            const CMUTIL_Socket *socket, uint8_t c);
};

/**
 * @brief Create a new socket which connected to the given endpoint.
 *
 * @param host host where connect to.
 * @param port port where connect to.
 * @param timeout connect timeout in milliseconds.
 * @return A connected socket if succeeded it must be closed after use.
 *      NULL if connect failed.
 */
CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnect(
        const char *host, int port, long timeout);

/**
 * @brief Create a new socket which connected to the given socket address.
 *
 * @param saddr socket address where connect to.
 * @param timeout connect timeout in milliseconds.
 * @return A connected socket if succeeded it must be closed after use.
 *      NULL if connect failed.
 */
CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnectWithAddr(
        const CMUTIL_SocketAddr *saddr, long timeout);

/**
 * @brief Create a new SSL socket which connected to the given endpoint.
 *
 * @param cert SSL certificate file path.
 * @param key SSL private key file path.
 * @param ca CA certificate file path.
 * @param servername Server name for SNI.
 * @param host host where connect to.
 * @param port port where connect to.
 * @param timeout connect timeout in milliseconds.
 * @return A connected SSL socket if succeeded it must be closed after use.
 *      NULL if connect failed.
 */
CMUTIL_API CMUTIL_Socket *CMUTIL_SSLSocketConnect(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout);

/**
 * @brief Create a new SSL socket which connected to the given socket address.
 *
 * @param cert SSL certificate file path.
 * @param key SSL private key file path.
 * @param ca CA certificate file path.
 * @param servername Server name for SNI.
 * @param saddr socket address where connect to.
 * @param timeout connect timeout in milliseconds.
 * @return A connected SSL socket if succeeded it must be closed after use.
 *      NULL if connect failed.
 */
CMUTIL_API CMUTIL_Socket *CMUTIL_SSLSocketConnectWithAddr(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const CMUTIL_SocketAddr *saddr, long timeout);

/**
 * @typedef CMUTIL_ServerSocket Server socket object.
 */
typedef struct CMUTIL_ServerSocket CMUTIL_ServerSocket;
struct CMUTIL_ServerSocket {

    /**
     * @brief Accept a new connection on the server socket.
     *
     * This function accepts a new incoming connection on the server socket
     * within the specified timeout period and returns the accepted socket.
     *
     * @param server The server socket object to accept connections from.
     * @param res Pointer to store the accepted socket object.
     * @param timeout The accept operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Accept)(
            const CMUTIL_ServerSocket *server,
            CMUTIL_Socket **res,
            long timeout);

    /**
     * @brief Close the server socket.
     *
     * This function closes the server socket and releases any associated resources.
     *
     * @param server The server socket object to be closed.
     */
    void (*Close)(CMUTIL_ServerSocket *server);
};

/**
 * @brief Create a new server socket which listens on the given host and port.
 *
 * @param host Host address to listen on.(0.0.0.0 for any address)
 * @param port Port number to listen on.
 * @param qcnt The maximum length of the queue of pending connections.
 * @return A server socket if succeeded it must be closed after use.
 *      NULL if failed.
 */
CMUTIL_API CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
        const char *host, int port, int qcnt);

/**
 * @brief Create a new SSL server socket which listens on the given host and port.
 *
 * @param cert SSL certificate file path.
 * @param key SSL private key file path.
 * @param ca CA certificate file path.
 * @param host Host address to listen on.(0.0.0.0 for any address)
 * @param port Port number to listen on.
 * @param qcnt The maximum length of the queue of pending connections.
 * @return AN SSL server socket if succeeded, it must be closed after use.
 *      NULL if failed.
 */
CMUTIL_API CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreate(
        const char *cert, const char *key, const char *ca,
        const char *host, int port, int qcnt);

/**
 * @brief Create a pair of connected sockets.
 *
 * @param s1 Socket pointer to store the first socket.
 * @param s2 Socket pointer to store the second socket.
 * @return CMTrue if succeeded, CMFalse if failed.
 */
CMUTIL_API CMBool CMUTIL_SocketPair(
        CMUTIL_Socket **s1, CMUTIL_Socket **s2);

/**
 * @typedef CMUTIL_DGramSocket Datagram socket object.
 */
typedef struct CMUTIL_DGramSocket CMUTIL_DGramSocket;
struct CMUTIL_DGramSocket {
    /**
     * @brief Bind the datagram socket to a specific address.
     *
     * @param dsock The datagram socket object to bind.
     * @param saddr The socket address to bind to.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Bind)(
            CMUTIL_DGramSocket *dsock,
            const CMUTIL_SocketAddr *saddr);

    /**
     * @brief Connect the datagram socket to a specific address.
     *
     * @param dsock The datagram socket object to connect.
     * @param saddr The socket address to connect to.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Connect)(
            CMUTIL_DGramSocket *dsock,
            const CMUTIL_SocketAddr *saddr);

    /**
     * @brief Check if the datagram socket is connected.
     *
     * @param dsock The datagram socket object to check.
     * @return CMTrue if the socket is connected, CMFalse otherwise.
     */
    CMBool (*IsConnected)(
            CMUTIL_DGramSocket *dsock);

    /**
     * @brief Disconnect the datagram socket.
     *
     * @param dsock The datagram socket object to disconnect.
     */
    void (*Disconnect)(
            CMUTIL_DGramSocket *dsock);

    /**
     * @brief Get the remote address of the datagram socket.
     *
     * @param dsock The datagram socket object.
     * @param saddr Pointer to the socket address object to store the remote address.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*GetRemoteAddr)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_SocketAddr *saddr);

    /**
     * @brief Get the local address of the datagram socket.
     *
     * @param dsock The datagram socket object.
     * @param saddr Pointer to the socket address object to store the local address.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*GetLocalAddr)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_SocketAddr *saddr);

    /**
     * @brief Send data to the connected remote address.
     *
     * To use this function, the datagram socket must be connected first.
     *
     * @param dsock The datagram socket object.
     * @param buf The byte buffer containing the data to send.
     * @param timeout The send operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Send)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            long timeout);

    /**
     * @brief Receive data from the connected remote address.
     *
     * To use this function, the datagram socket must be connected first.
     *
     * @param dsock The datagram socket object.
     * @param buf The byte buffer to store the received data.
     * @param timeout The receive operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*Recv)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            long timeout);

    /**
     * @brief Send data to a specific address.
     *
     * @param dsock The datagram socket object.
     * @param buf The byte buffer containing the data to send.
     * @param saddr The socket address to send the data to.
     * @param timeout The send operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*SendTo)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            CMUTIL_SocketAddr *saddr,
            long timeout);

    /**
     * @brief Receive data from a specific address.
     *
     * @param dsock The datagram socket object.
     * @param buf The byte buffer to store the received data.
     * @param saddr Pointer to the socket address object to store the sender's address.
     * @param timeout The receive operation timeout in milliseconds.
     * @return CMSocketResult indicating success or failure.
     */
    CMSocketResult (*RecvFrom)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            CMUTIL_SocketAddr *saddr,
            long timeout);

    /**
     * @brief Close the datagram socket.
     *
     * This function closes the datagram socket and releases any associated resources.
     *
     * @param dsock The datagram socket object to be closed.
     */
    void (*Close)(
            CMUTIL_DGramSocket *dsock);
};

/**
 * @brief Create a new datagram socket.
 *
 * @return A datagram socket if succeeded it must be closed after use.
 */
CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreate(void);

/**
 * @brief Create a new datagram socket and bind it to the given address.
 *
 * @param addr Socket address to bind the datagram socket to.
 * @return A bound datagram socket if succeeded it must be closed after use.
 */
CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreateBind(
        CMUTIL_SocketAddr *addr);


/**
 * @typedef CMJsonType JSON value types.
 */
typedef enum CMJsonType {
    /** JSON value type. */
    CMJsonTypeValue = 0,
    /** JSON object type. */
    CMJsonTypeObject,
    /** JSON array type. */
    CMJsonTypeArray
} CMJsonType;

/**
 * @typedef CMUTIL_Json JSON base object.
 */
typedef struct CMUTIL_Json CMUTIL_Json;
struct CMUTIL_Json {
    /**
     * @brief Convert the JSON object to a string representation.
     *
     * This method serializes the JSON object into a string format.
     * If 'pretty' is CMTrue, the output will be formatted with
     * indentation and line breaks for better readability.
     *
     * @param json The JSON object to be converted.
     * @param buf The string buffer to store the resulting string.
     * @param pretty If CMTrue, the output will be pretty-printed.
     */
    void (*ToString)(
            const CMUTIL_Json *json, CMUTIL_String *buf, CMBool pretty);

    /**
     * @brief Get the type of the JSON object.
     *
     * This method retrieves the type of the JSON object, which can be
     * one of the following: CMJsonTypeValue, CMJsonTypeObject, or CMJsonTypeArray.
     *
     * @param json The JSON object to get the type from.
     * @return The type of the JSON object.
     */
    CMJsonType (*GetType)(
            const CMUTIL_Json *json);

    /**
     * @brief Clone the JSON object.
     *
     * This method creates a deep copy of the JSON object.
     *
     * @param json The JSON object to be cloned.
     * @return A new JSON object that is a clone of the original.
     */
    CMUTIL_Json *(*Clone)(
            const CMUTIL_Json *json);

    /**
     * @brief Destroy the JSON object.
     *
     * This method releases all resources associated with the JSON object
     * and frees any allocated memory.
     *
     * @param json A pointer to the CMUTIL_Json object to be destroyed.
     */
    void (*Destroy)(CMUTIL_Json *json);
};

/**
 * @brief Destroy the JSON object.
 *
 * This macro provides a convenient way to call the Destroy method
 * of a CMUTIL_Json variant.
 *
 * @param a A pointer to the CMUTIL_Json variant to be destroyed.
 */
#define CMUTIL_JsonDestroy(a)   CMCall((CMUTIL_Json*)(a), Destroy)

/**
 * @typedef CMJsonValueType JSON value types.
 */
typedef enum CMJsonValueType {
    /** JSON long integer type. */
    CMJsonValueLong = 0,
    /** JSON double type. */
    CMJsonValueDouble,
    /** JSON string type. */
    CMJsonValueString,
    /** JSON boolean type. */
    CMJsonValueBoolean,
    /** JSON null type. */
    CMJsonValueNull
} CMJsonValueType;

/**
 * @typedef CMUTIL_JsonValue JSON value object.
 *
 * Must be destroyed with <code>CMUTIL_JsonDestroy</code>.
 */
typedef struct CMUTIL_JsonValue CMUTIL_JsonValue;
struct CMUTIL_JsonValue {
    /** JSON base object. */
    CMUTIL_Json parent;

    /**
     * @brief Get the type of the JSON value.
     *
     * This method retrieves the type of the JSON value, which can be
     * one of the following: CMJsonValueLong, CMJsonValueDouble,
     * CMJsonValueString, CMJsonValueBoolean, or CMJsonValueNull.
     *
     * @param jval The JSON value object to get the type from.
     * @return The type of the JSON value.
     */
    CMJsonValueType (*GetValueType)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Get the long integer value from the JSON value.
     *
     * This method retrieves the long integer value stored in the JSON value object.
     *
     * @param jval The JSON value object to get the long integer from.
     * @return The long integer value.
     */
    int64_t (*GetLong)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Get the double value from the JSON value.
     *
     * This method retrieves the double value stored in the JSON value object.
     *
     * @param jval The JSON value object to get the double from.
     * @return The double value.
     */
    double (*GetDouble)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Get the string value from the JSON value.
     *
     * This method retrieves the string value stored in the JSON value object.
     *
     * @param jval The JSON value object to get the string from.
     * @return A pointer to the CMUTIL_String containing the string value.
     */
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Get the C-style string value from the JSON value.
     *
     * This method retrieves the C-style string value stored in the JSON value object.
     *
     * @param jval The JSON value object to get the C-style string from.
     * @return A pointer to the C-style string.
     */
    const char *(*GetCString)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Get the boolean value from the JSON value.
     *
     * This method retrieves the boolean value stored in the JSON value object.
     *
     * @param jval The JSON value object to get the boolean from.
     * @return The boolean value.
     */
    CMBool (*GetBoolean)(
            const CMUTIL_JsonValue *jval);

    /**
     * @brief Set the long integer value of the JSON value.
     *
     * This method sets the long integer value of the JSON value object.
     *
     * @param jval The JSON value object to set the long integer for.
     * @param inval The long integer value to set.
     */
    void (*SetLong)(
            CMUTIL_JsonValue *jval, int64_t inval);

    /**
     * @brief Set the double value of the JSON value.
     *
     * This method sets the double value of the JSON value object.
     *
     * @param jval The JSON value object to set the double for.
     * @param inval The double value to set.
     */
    void (*SetDouble)(
            CMUTIL_JsonValue *jval, double inval);

    /**
     * @brief Set the string value of the JSON value.
     *
     * This method sets the string value of the JSON value object.
     *
     * @param jval The JSON value object to set the string for.
     * @param inval The string value to set.
     */
    void (*SetString)(
            CMUTIL_JsonValue *jval, const char *inval);

    /**
     * @brief Set the boolean value of the JSON value.
     *
     * This method sets the boolean value of the JSON value object.
     *
     * @param jval The JSON value object to set the boolean for.
     * @param inval The boolean value to set.
     */
    void (*SetBoolean)(
            CMUTIL_JsonValue *jval, CMBool inval);

    /**
     * @brief Set the JSON value to null.
     *
     * This method sets the JSON value object to represent a null value.
     *
     * @param jval The JSON value object to set to null.
     */
    void (*SetNull)(
            CMUTIL_JsonValue *jval);
};

/**
 * @brief Create a new JSON value object.
 *
 * @return A pointer to the newly created CMUTIL_JsonValue object,
 */
CMUTIL_API CMUTIL_JsonValue *CMUTIL_JsonValueCreate(void);

/**
 * @typedef CMUTIL_JsonObject JSON object.
 *
 * Must be destroyed with <code>CMUTIL_JsonDestroy</code>.
 */
typedef struct CMUTIL_JsonObject CMUTIL_JsonObject;
struct CMUTIL_JsonObject {
    /** JSON base object. */
    CMUTIL_Json parent;

    /**
     * @brief Get the keys of the JSON object.
     *
     * This method retrieves the keys of the JSON object as a string array.
     * The caller must destroy the returned CMUTIL_StringArray after use.
     *
     * @param jobj The JSON object to get the keys from.
     * @return A pointer to a CMUTIL_StringArray containing the keys.
     */
    CMUTIL_StringArray *(*GetKeys)(
            const CMUTIL_JsonObject *jobj);

    /**
     * @brief Get the value associated with a key in the JSON object.
     *
     * This method retrieves the value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the value from.
     * @param key The key whose associated value is to be retrieved.
     * @return A pointer to the CMUTIL_Json value associated with the key.
     */
    CMUTIL_Json *(*Get)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Get the long integer value associated with a key in the JSON object.
     *
     * This method retrieves the long integer value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the long integer from.
     * @param key The key whose associated long integer value is to be retrieved.
     * @return The long integer value associated with the key.
     */
    int64_t (*GetLong)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Get the double value associated with a key in the JSON object.
     *
     * This method retrieves the double value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the double from.
     * @param key The key whose associated double value is to be retrieved.
     * @return The double value associated with the key.
     */
    double (*GetDouble)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Get the string value associated with a key in the JSON object.
     *
     * This method retrieves the string value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the string from.
     * @param key The key whose associated string value is to be retrieved.
     * @return A pointer to the CMUTIL_String containing the string value.
     */
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Get the C-style string value associated with a key in the JSON object.
     *
     * This method retrieves the C-style string value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the C-style string from.
     * @param key The key whose associated C-style string value is to be retrieved.
     * @return A pointer to the C-style string.
     */
    const char *(*GetCString)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Get the boolean value associated with a key in the JSON object.
     *
     * This method retrieves the boolean value associated with the specified key
     * in the JSON object.
     *
     * @param jobj The JSON object to get the boolean value from.
     * @param key The key whose associated boolean value is to be retrieved.
     * @return CMTrue if the value is true, CMFalse otherwise.
     */
    CMBool (*GetBoolean)(
            const CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Put a JSON object into the JSON object with a key.
     *
     * This method adds or updates a JSON object with the specified key in the
     * JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the JSON value into.
     * @param key The key to associate with the JSON value.
     * @param json The JSON value to be put.
     */
    void (*Put)(
            CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Json *json);

    /**
     * @brief Put a long integer value into the JSON object with a key.
     *
     * This method adds or updates a long integer value with the specified key
     * in the JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the long integer value into.
     * @param key The key to associate with the long integer value.
     * @param value The long integer value to be put.
     */
    void (*PutLong)(
            CMUTIL_JsonObject *jobj, const char *key, int64_t value);

    /**
     * @brief Put a double value into the JSON object with a key.
     *
     * This method adds or updates a double value with the specified key
     * in the JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the double value into.
     * @param key The key to associate with the double value.
     * @param value The double value to be put.
     */
    void (*PutDouble)(
            CMUTIL_JsonObject *jobj, const char *key, double value);

    /**
     * @brief Put a string value into the JSON object with a key.
     *
     * This method adds or updates a string value with the specified key
     * in the JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the string value into.
     * @param key The key to associate with the string value.
     * @param value The string value to be put.
     */
    void (*PutString)(
            CMUTIL_JsonObject *jobj, const char *key, const char *value);

    /**
     * @brief Put a boolean value into the JSON object with a key.
     *
     * This method adds or updates a boolean value with the specified key
     * in the JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the boolean value into.
     * @param key The key to associate with the boolean value.
     * @param value The boolean value to be put.
     */
    void (*PutBoolean)(
            CMUTIL_JsonObject *jobj, const char *key, CMBool value);

    /**
     * @brief Put a null value into the JSON object with a key.
     *
     * This method adds or updates a null value with the specified key
     * in the JSON object. If the key already exists, the existing value is replaced.
     *
     * @param jobj The JSON object to put the null value into.
     * @param key The key to associate with the null value.
     */
    void (*PutNull)(
            CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Remove a key-value pair from the JSON object.
     *
     * This method removes the key-value pair associated with the specified key
     * from the JSON object. If the key does not exist, no action is taken.
     * Ownership of the removed JSON value is transferred to the caller.
     *
     * @param jobj The JSON object to remove the key-value pair from.
     * @param key The key to remove.
     * @return The removed JSON value, or NULL if the key does not exist.
     */
    CMUTIL_Json *(*Remove)(
            CMUTIL_JsonObject *jobj, const char *key);

    /**
     * @brief Delete a key-value pair from the JSON object.
     *
     * This method deletes the key-value pair associated with the specified key
     * from the JSON object. If the key does not exist, no action is taken.
     *
     * @param jobj The JSON object to delete the key-value pair from.
     * @param key The key to delete.
     */
    void (*Delete)(
            CMUTIL_JsonObject *jobj, const char *key);
};

/**
 * @brief Create a new JSON object.
 *
 * This function creates a new JSON object and returns a pointer to it.
 *
 * @return A pointer to the newly created JSON object, or NULL on failure.
 */
CMUTIL_API CMUTIL_JsonObject *CMUTIL_JsonObjectCreate(void);

/**
 * @typedef CMUTIL_JsonArray JSON array type.
 *
 * Must be destroyed with <code>CMUTIL_JsonDestroy</code>.
 */
typedef struct CMUTIL_JsonArray CMUTIL_JsonArray;
struct CMUTIL_JsonArray {
    /** JSON base object */
    CMUTIL_Json parent;

    /**
     * @brief Get the size of the JSON array.
     *
     * This method returns the number of elements in the JSON array.
     *
     * @param jarr The JSON array to get the size from.
     * @return The size of the JSON array.
     */
    size_t (*GetSize)(
            const CMUTIL_JsonArray *jarr);

    /**
     * @brief Get a JSON value from the JSON array by index.
     *
     * This method retrieves the JSON value at the specified index in the JSON array.
     *
     * @param jarr The JSON array to get the JSON value from.
     * @param index The index of the JSON value to retrieve.
     * @return A pointer to the JSON value at the specified index,
     *         or NULL if the index is out of bounds.
     */
    CMUTIL_Json *(*Get)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Get a long integer value from the JSON array by index.
     *
     * This method retrieves the long integer value at the specified index in the JSON array.
     *
     * @param jarr The JSON array to get the long integer value from.
     * @param index The index of the long integer value to retrieve.
     * @return The long integer value at the specified index,
     *         or 0 if the index is out of bounds or the value is not a long integer.
     */
    int64_t (*GetLong)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Get a double value from the JSON array by index.
     *
     * This method retrieves the double value at the specified index in the JSON array.
     *
     * @param jarr The JSON array to get the double value from.
     * @param index The index of the double value to retrieve.
     * @return The double value at the specified index,
     *         or 0.0 if the index is out of bounds or the value is not a double.
     */
    double (*GetDouble)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Get a string value from the JSON array by index.
     *
     * This method retrieves the string value at the specified index in the JSON array.
     * Ownership of the returned string is not transferred to the caller.
     *
     * @param jarr The JSON array to get the string value from.
     * @param index The index of the string value to retrieve.
     * @return A pointer to the string value at the specified index,
     *         or NULL if the index is out of bounds or the value is not a string.
     */
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Get a C-style string value from the JSON array by index.
     *
     * This method retrieves the C-style string value at the specified index in the JSON array.
     * Ownership of the returned string is not transferred to the caller.
     *
     * @param jarr The JSON array to get the C-style string value from.
     * @param index The index of the C-style string value to retrieve.
     * @return A pointer to the C-style string value at the specified index,
     *         or NULL if the index is out of bounds or the value is not a C-style string.
     */
    const char *(*GetCString)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Get a boolean value from the JSON array by index.
     *
     * This method retrieves the boolean value at the specified index in the JSON array.
     *
     * @param jarr The JSON array to get the boolean value from.
     * @param index The index of the boolean value to retrieve.
     * @return The boolean value at the specified index,
     *         or CMFalse if the index is out of bounds or the value is not a boolean.
     */
    CMBool (*GetBoolean)(
            const CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Add a JSON value to the JSON array.
     *
     * This method appends a JSON value to the end of the JSON array.
     * Ownership of the JSON value is transferred to the JSON array.
     *
     * @param jarr The JSON array to add the JSON value to.
     * @param json The JSON value to add.
     */
    void (*Add)(
            CMUTIL_JsonArray *jarr, CMUTIL_Json *json);

    /**
     * @brief Add a long integer value to the JSON array.
     *
     * This method appends a long integer value to the end of the JSON array.
     *
     * @param jarr The JSON array to add the long integer value to.
     * @param value The long integer value to add.
     */
    void (*AddLong)(
            CMUTIL_JsonArray *jarr, int64_t value);

    /**
     * @brief Add a double value to the JSON array.
     *
     * This method appends a double value to the end of the JSON array.
     *
     * @param jarr The JSON array to add the double value to.
     * @param value The double value to add.
     */
    void (*AddDouble)(
            CMUTIL_JsonArray *jarr, double value);

    /**
     * @brief Add a string value to the JSON array.
     *
     * This method appends a string value to the end of the JSON array.
     * Ownership of the string value is not transferred to the JSON array.
     *
     * @param jarr The JSON array to add the string value to.
     * @param value The string value to add.
     */
    void (*AddString)(
            CMUTIL_JsonArray *jarr, const char *value);

    /**
     * @brief Add a boolean value to the JSON array.
     *
     * This method appends a boolean value to the end of the JSON array.
     *
     * @param jarr The JSON array to add the boolean value to.
     * @param value The boolean value to add.
     */
    void (*AddBoolean)(
            CMUTIL_JsonArray *jarr, CMBool value);

    /**
     * @brief Add a null value to the JSON array.
     *
     * This method appends a null value to the end of the JSON array.
     *
     * @param jarr The JSON array to add the null value to.
     */
    void (*AddNull)(
            CMUTIL_JsonArray *jarr);

    /**
     * @brief Remove a JSON value from the JSON array by index.
     *
     * This method removes the JSON value at the specified index from the JSON array.
     * Ownership of the removed JSON value is transferred to the caller.
     *
     * @param jarr The JSON array to remove the JSON value from.
     * @param index The index of the JSON value to remove.
     * @return The removed JSON value, or NULL if the index is out of bounds.
     */
    CMUTIL_Json *(*Remove)(
            CMUTIL_JsonArray *jarr, uint32_t index);

    /**
     * @brief Delete a JSON value from the JSON array by index.
     *
     * This method removes the JSON value at the specified index from the JSON array.
     * Ownership of the removed JSON value is not transferred to the caller.
     *
     * @param jarr The JSON array to delete the JSON value from.
     * @param index The index of the JSON value to delete.
     */
    void (*Delete)(
            CMUTIL_JsonArray *jarr, uint32_t index);
};

/**
 * @brief Create a new JSON array.
 *
 * @return A new JSON array, or NULL if memory allocation fails.
 */
CMUTIL_API CMUTIL_JsonArray *CMUTIL_JsonArrayCreate(void);

/**
 * @brief Parse a JSON string into a JSON object.
 *
 * @param jsonstr The JSON string to parse.
 * @return A new JSON object, or NULL if parsing fails.
 */
CMUTIL_API CMUTIL_Json *CMUTIL_JsonParse(CMUTIL_String *jsonstr);

/**
 * @brief Convert an XML node to a JSON object.
 *
 * @param node The XML node to convert.
 * @return A new JSON object, or NULL if conversion fails.
 */
CMUTIL_API CMUTIL_Json *CMUTIL_XmlToJson(CMUTIL_XmlNode *node);


/**
 * @typedef Enumeration of process stream types.
 */
typedef enum CMProcStreamType {
    /** process with no stream */
    CMProcStreamNone = 0,
    /** process with read stream */
    CMProcStreamRead = 1,
    /** process with writing stream */
    CMProcStreamWrite = 2,
    /** process with read and write streams */
    CMProcStreamReadWrite = 3
} CMProcStreamType;

/**
 * @typedef Process structure for managing external processes.
 */
typedef struct CMUTIL_Process CMUTIL_Process;
struct CMUTIL_Process {
    /**
     * @brief Start the process.
     *
     * If the process is already running, this function will return CMFalse.
     *
     * @param proc The process to start.
     * @param type The type of stream to use for the process.
     * @return CMTrue if the process started successfully, CMFalse otherwise.
     */
    CMBool (*Start)(CMUTIL_Process *proc, CMProcStreamType type);

    /**
     * @brief Retrieves the process identifier (PID) of the specified process.
     *
     * This function returns the PID of the process represented by the given
     * `CMUTIL_Process` instance. If the process is not running or is in an
     * invalid state, the behavior is implementation-defined.
     *
     * @param proc The process from which to retrieve the PID.
     * @return The PID of the specified process, or an appropriate error
     *         indicator if the PID cannot be retrieved.
     */
    pid_t (*GetPid)(CMUTIL_Process *proc);

    /**
     * @brief Function pointer to retrieve the command associated with a given process.
     *
     * This variable points to a function that, given a process instance, returns
     * the associated command in the form of a null-terminated string.
     *
     * @param proc Pointer to a CMUTIL_Process instance representing the process.
     * @return const char* Null-terminated string representing the command associated
     *         with the specified process. Returns nullptr if the command cannot be
     *         retrieved or is not available.
     */
    const char *(*GetCommand)(CMUTIL_Process *proc);

    /**
     * @brief Function pointer to retrieve the working directory of a given process.
     *
     * This function takes a pointer to a CMUTIL_Process structure and returns a
     * constant character pointer representing the working directory of the
     * specified process.
     *
     * @param proc A pointer to a CMUTIL_Process structure representing the process
     *             whose working directory is to be retrieved.
     * @return A constant character pointer pointing to the working directory of
     *         the given process.
     */
    const char *(*GetWorkingDirectory)(CMUTIL_Process *proc);

    /**
     * @brief Function pointer to retrieve the argument list of a process.
     *
     * This function pointer takes a CMUTIL_Process object as input and
     * returns a pointer to a CMUTIL_StringArray containing the arguments
     * associated with the specified process.
     *
     * @param proc A pointer to a CMUTIL_Process object whose arguments
     *             are to be retrieved.
     * @return A pointer to a CMUTIL_StringArray containing the process arguments.
     */
    const CMUTIL_StringArray *(*GetArgs)(CMUTIL_Process *proc);

    /**
     * @brief Retrieves the environment map for a specified process.
     *
     * This function pointer allows access to the environment variables
     * associated with the given process.
     *
     * @param proc A pointer to a CMUTIL_Process object whose environment
     *             variables are to be retrieved.
     * @return A pointer to a CMUTIL_Map containing the process environment variables.
     */
    const CMUTIL_Map *(*GetEnv)(CMUTIL_Process *proc);

    /**
     * @brief Suspends the execution of the specified process.
     *
     * This function halts the execution of the given process until it is explicitly
     * resumed. If the process is not running or is in an invalid state, this
     * function does nothing and returns without error.
     *
     * @param proc The process to suspend.
     */
    void (*Suspend)(CMUTIL_Process *proc);

    /**
     * @brief Resumes the execution of the specified process.
     *
     * This function resumes a previously suspended process, allowing it to continue
     * its execution. If the process is not in a suspended state or is in an invalid
     * state, this function does nothing and returns without error.
     *
     * @param proc The process to resume.
     */
    void (*Resume)(CMUTIL_Process *proc);

    /**
     * @brief Establishes a unidirectional pipe between two processes.
     *
     * This function creates a communication channel between the stdout
     * of the source process (`proc`) and the stdin of the target process (`target`).
     * It facilitates data transfer from one process to another. Both `proc` and
     * `target` must be valid, and the state of both processes must not be
     * started for the operation to succeed.
     * This process must be started with CMProcStreamWrite or CMProcStreamReadWrite
     * stream type, and the target process must be started with CMProcStreamRead
     * or CMProcStreamReadWrite stream type.
     *
     * @param proc The source process whose stdout will be piped.
     * @param target The target process whose stdin will receive the piped data.
     * @return CMTrue if the pipe was successfully established, CMFalse otherwise.
     */
    CMBool (*PipeTo)(CMUTIL_Process *proc, CMUTIL_Process *target);

    /**
     * @brief Write data to the process's stdin.
     *
     * If the process is not running, or the stream type is
     * not CMProcStreamWrite or CMProcStreamReadWrite,
     * this function will return -1.
     *
     * @param proc The process to write to.
     * @param buf The buffer containing data to write.
     * @param count The number of bytes to write.
     * @return The number of bytes written, or -1 on error.
     */
    int (*Write)(CMUTIL_Process *proc, const void *buf, size_t count);

    /**
     * @brief Read data from the process's stdout.
     *
     * If the process is not running, or the stream type is
     * not CMProcStreamRead or CMProcStreamReadWrite,
     * this function will return -1.
     *
     * @param proc The process to read from.
     * @param buf The buffer to store the read data.
     * @param count The maximum number of bytes to read.
     * @return The number of bytes read, or -1 on error.
     */
    int (*Read)(CMUTIL_Process *proc, CMUTIL_ByteBuffer *buf, size_t count);

    /**
     * @brief Waits for the specified process to complete.
     *
     * This function blocks the calling thread until the specified process
     * finishes execution.
     *
     * @param proc The process to wait for.
     * @param millis The maximum time to wait in milliseconds,
     *               or -1 to wait infinitely.
     * @return An exit status code from the process, or -1 on error.
     */
    int (*Wait)(CMUTIL_Process *proc, long millis);

    /**
     * @brief Terminates the specified process.
     *
     * This function attempts to terminate the specified process.
     * If the process is not running, this function will do nothing.
     *
     * @param proc The process to terminate.
     */
    void (*Kill)(CMUTIL_Process *proc);

    /**
     * @brief Frees resources associated with the specified process.
     *
     * This function releases any memory or resources allocated for the given
     * `CMUTIL_Process` instance. After this function is called, the specified
     * process object should no longer be used.
     *
     * @param proc The process whose resources should be freed.
     */
    void (*Destroy)(CMUTIL_Process *proc);
};

/**
 * @brief Creates a new process with the specified command and stream type.
 *
 * @param cwd The working directory for the new process.
 * @param env The environment variables for the new process.
 * @param command The command to execute in the new process.
 * @param ... Additional arguments of the process.
 * @return A pointer to the newly created process, or NULL on failure.
 */
CMUTIL_API CMUTIL_Process *CMUTIL_ProcessCreate(
        const char *cwd,
        CMUTIL_Map *env,
        const char *command,
        ...);

#ifdef __cplusplus
}
#endif

#endif // LIBCMUTILS_H__

