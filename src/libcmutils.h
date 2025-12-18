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
 *
 * Copyright 2016 Dennis Soungjin Park <xcomart@gmail.com>, all rights reserved.
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

#define __STDC_FORMAT_MACROS 1 /* for 64bit integer related macros */
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
 * to remove anoying warning in qtcreator
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
 * CMUTIL library uses platform independent data types for multi-platform
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
 * 64 bit signed / unsigned integer constant suffix.
 */
#define CMUTIL_APPEND(a,b)  a ## b
#if defined(_MSC_VER)
# define i64(x)     CMUTIL_APPEND(x, i64)
# define ui64(x)    CMUTIL_APPEND(x, ui64)
#else
# define i64(x)     CMUTIL_APPEND(x, LL)
# define ui64(x)    CMUTIL_APPEND(x, ULL)
#endif

/**
 * @brief Boolean definition for this library.
 */
typedef enum CMBool {
    CMTrue         = 1,
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
    /**
     * Use system version malloc, realloc, strdup, and free.
     */
    CMMemSystem = 0,
    /**
     * Use memory recycle technique.
     */
    CMMemRecycle,
    /**
     * Use memory operation with debugging information.
     */
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
    void *(*Calloc)(
            size_t nmem,
            size_t size);
    void *(*Realloc)(
            void *ptr,
            size_t size);
    char *(*Strdup)(
            const char *str);
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
     * The order of the items in the map is preserved.
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



typedef struct CMUTIL_List CMUTIL_List;
struct CMUTIL_List {
    void (*AddFront)(CMUTIL_List *list, void *data);
    void (*AddTail)(CMUTIL_List *list, void *data);
    void *(*GetFront)(const CMUTIL_List *list);
    void *(*GetTail)(const CMUTIL_List *list);
    void *(*RemoveFront)(CMUTIL_List *list);
    void *(*RemoveTail)(CMUTIL_List *list);
    void *(*Remove)(CMUTIL_List *list, void *data);
    size_t (*GetSize)(const CMUTIL_List *list);
    CMUTIL_Iterator *(*Iterator)(const CMUTIL_List *list);
    void (*Destroy)(CMUTIL_List *list);
};

#define CMUTIL_ListCreate() CMUTIL_ListCreateEx(NULL)
CMUTIL_API CMUTIL_List *CMUTIL_ListCreateEx(CMFreeCB freecb);





typedef enum CMXmlNodeKind {
    CMXmlNodeUnknown = 0,
    CMXmlNodeText,
    CMXmlNodeTag
} CMXmlNodeKind;

typedef struct CMUTIL_XmlNode CMUTIL_XmlNode;
struct CMUTIL_XmlNode {
    CMUTIL_StringArray *(*GetAttributeNames)(const CMUTIL_XmlNode *node);
    CMUTIL_String *(*GetAttribute)(
            const CMUTIL_XmlNode *node, const char *key);
    void (*SetAttribute)(
            CMUTIL_XmlNode *node, const char *key, const char *value);
    size_t (*ChildCount)(const CMUTIL_XmlNode *node);
    void (*AddChild)(CMUTIL_XmlNode *node, CMUTIL_XmlNode *child);
    CMUTIL_XmlNode *(*ChildAt)(const CMUTIL_XmlNode *node, uint32_t index);
    CMUTIL_XmlNode *(*GetParent)(const CMUTIL_XmlNode *node);
    const char *(*GetName)(const CMUTIL_XmlNode *node);
    void (*SetName)(CMUTIL_XmlNode *node, const char *name);
    CMXmlNodeKind (*GetType)(const CMUTIL_XmlNode *node);
    CMUTIL_String *(*ToDocument)(
            const CMUTIL_XmlNode *node, CMBool beutify);
    void (*SetUserData)(
            CMUTIL_XmlNode *node, void *udata, void(*freef)(void*));
    void *(*GetUserData)(const CMUTIL_XmlNode *node);
    void (*Destroy)(CMUTIL_XmlNode *node);
};

CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParse(CMUTIL_String *str);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseString(
        const char *xmlstr, size_t len);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseFile(const char *fpath);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreate(
        CMXmlNodeKind type, const char *tagname);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
        CMXmlNodeKind type, const char *tagname, size_t len);




typedef struct CMUTIL_CSConv CMUTIL_CSConv;
struct CMUTIL_CSConv {
    CMUTIL_String *(*Forward)(
            const CMUTIL_CSConv *conv, CMUTIL_String *instr);
    CMUTIL_String *(*Backward)(
            const CMUTIL_CSConv *conv, CMUTIL_String *instr);
    void (*Destroy)(CMUTIL_CSConv *conv);
};

CMUTIL_API CMUTIL_CSConv *CMUTIL_CSConvCreate(
        const char *fromcs, const char *tocs);



typedef struct CMUTIL_TimerTask CMUTIL_TimerTask;
struct CMUTIL_TimerTask {
    void (*Cancel)(CMUTIL_TimerTask *task);
};

typedef struct CMUTIL_Timer CMUTIL_Timer;
struct CMUTIL_Timer {
    CMUTIL_TimerTask *(*ScheduleAtTime)(
            CMUTIL_Timer *timer, struct timeval *attime,
            CMProcCB proc, void *param);
    CMUTIL_TimerTask *(*ScheduleDelay)(
            CMUTIL_Timer *timer, long delay,
            CMProcCB proc, void *param);
    CMUTIL_TimerTask *(*ScheduleAtRepeat)(
            CMUTIL_Timer *timer, struct timeval *first, long period,
            CMProcCB proc, void *param);
    CMUTIL_TimerTask *(*ScheduleDelayRepeat)(
            CMUTIL_Timer *timer, long delay, long period,
            CMProcCB proc, void *param);
    void (*Purge)(CMUTIL_Timer *timer);
    void (*Destroy)(CMUTIL_Timer *timer);
};

#define CMUTIL_TIMER_PRECISION  100
#define CMUTIL_TIMER_THREAD     10
#define CMUTIL_TimerCreate()    \
        CMUTIL_TimerCreateEx(CMUTIL_TIMER_PRECISION, CMUTIL_TIMER_THREAD)
CMUTIL_API CMUTIL_Timer *CMUTIL_TimerCreateEx(long precision, int threads);


typedef struct CMUTIL_Pool CMUTIL_Pool;
struct CMUTIL_Pool {
    void *(*CheckOut)(CMUTIL_Pool *pool, long millisec);
    void (*Release)(CMUTIL_Pool *pool, void *resource);
    void (*AddResource)(CMUTIL_Pool *pool, void *resource);
    void (*Destroy)(CMUTIL_Pool *pool);
};

typedef void* (*CMPoolItemCreateCB)(void *udata);
typedef void (*CMPoolItemFreeCB)(void *resource, void *udata);
typedef CMBool (*CMPoolItemTestCB)(void *resource, void *udata);

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




typedef struct CMUTIL_Library CMUTIL_Library;
struct CMUTIL_Library {
    void *(*GetProcedure)(
            const CMUTIL_Library *lib,
            const char *proc_name);
    void (*Destroy)(
            CMUTIL_Library *lib);
};

CMUTIL_API CMUTIL_Library *CMUTIL_LibraryCreate(const char *path);


typedef struct CMUTIL_FileList CMUTIL_FileList;
typedef struct CMUTIL_File CMUTIL_File;
struct CMUTIL_FileList {
    size_t (*Count)(
            const CMUTIL_FileList *flist);
    CMUTIL_File *(*GetAt)(
            const CMUTIL_FileList *flist,
            uint32_t index);
    void (*Destroy)(
            CMUTIL_FileList *flist);
};

struct CMUTIL_File {
    CMUTIL_String *(*GetContents)(
            const CMUTIL_File *file);
    CMBool (*Delete)(
            const CMUTIL_File *file);
    CMBool (*IsFile)(
            const CMUTIL_File *file);
    CMBool (*IsDirectory)(
            const CMUTIL_File *file);
    CMBool (*IsExists)(
            const CMUTIL_File *file);
    long (*Length)(
            const CMUTIL_File *file);
    const char *(*GetName)(
            const CMUTIL_File *file);
    const char *(*GetFullPath)(
            const CMUTIL_File *file);
    time_t (*ModifiedTime)(
            const CMUTIL_File *file);
    CMUTIL_FileList *(*Children)(
            const CMUTIL_File *file);
    /**
     * @brief Find all children which name is matched with given pattern.
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
     */
    CMUTIL_FileList *(*Find)(
            const CMUTIL_File *file,
            const char *pattern, CMBool recursive);
    void (*Destroy)(
            CMUTIL_File *file);
};

CMUTIL_API CMUTIL_File *CMUTIL_FileCreate(const char *path);

CMUTIL_API CMBool CMUTIL_PathCreate(const char *path, uint32_t mode);

typedef struct CMUTIL_Config CMUTIL_Config;
struct CMUTIL_Config {
    void (*Save)(const CMUTIL_Config *conf, const char *path);
    const char *(*Get)(const CMUTIL_Config *conf, const char *key);
    void (*Set)(CMUTIL_Config *conf, const char *key, const char *value);
    long (*GetLong)(const CMUTIL_Config *conf, const char *key);
    void (*SetLong)(CMUTIL_Config *conf, const char *key, long value);
    double (*GetDouble)(const CMUTIL_Config *conf, const char *key);
    void (*SetDouble)(CMUTIL_Config *conf, const char *key, double value);
    CMBool (*GetBoolean)(const CMUTIL_Config *conf, const char *key);
    void (*SetBoolean)(CMUTIL_Config *conf, const char *key, CMBool value);
    void(*Destroy)(CMUTIL_Config *conf);
};

CMUTIL_API CMUTIL_Config *CMUTIL_ConfigCreate(void);
CMUTIL_API CMUTIL_Config *CMUTIL_ConfigLoad(const char *fconf);


#define CMUTIL_LOG_CONFIG_DEFAULT       "cmutil_log.jsonc"

typedef enum CMLogLevel {
    CMLogLevel_Trace = 0,
    CMLogLevel_Debug,
    CMLogLevel_Info,
    CMLogLevel_Warn,
    CMLogLevel_Error,
    CMLogLevel_Fatal
} CMLogLevel;

typedef enum CMLogTerm {
    CMLogTerm_Year = 0,
    CMLogTerm_Month,
    CMLogTerm_Date,
    CMLogTerm_Hour,
    CMLogTerm_Minute
} CMLogTerm;

typedef struct CMUTIL_LogAppender CMUTIL_LogAppender;

typedef struct CMUTIL_ConfLogger CMUTIL_ConfLogger;
struct CMUTIL_ConfLogger {
    void(*AddAppender)(
        const CMUTIL_ConfLogger *logger,
        CMUTIL_LogAppender *appender,
        CMLogLevel level);
};

typedef struct CMUTIL_Logger CMUTIL_Logger;
struct CMUTIL_Logger {
    void(*LogEx)(
        CMUTIL_Logger *logger,
        CMLogLevel level,
        const char *file,
        int line,
        CMBool printStack,
        const char *fmt,
        ...);
    CMBool(*IsEnabled)(
            CMUTIL_Logger *logger,
            CMLogLevel level);
};

struct CMUTIL_LogAppender {
    const char *(*GetName)(
            const CMUTIL_LogAppender *appender);
    void(*SetAsync)(
            CMUTIL_LogAppender *appender, size_t buffersz);
    void(*Append)(
        CMUTIL_LogAppender *appender,
        CMUTIL_Logger *logger,
        CMLogLevel level,
        CMUTIL_StringArray *stack,
        const char *file,
        int line,
        CMUTIL_String *logmsg);
    void(*Flush)(CMUTIL_LogAppender *appender);
    void(*Destroy)(CMUTIL_LogAppender *appender);
};

CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogConsoleAppenderCreate(
    const char *name,
    const char *pattern);
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogFileAppenderCreate(
    const char *name,
    const char *fpath,
    const char *pattern);
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogRollingFileAppenderCreate(
    const char *name,
    const char *fpath,
    CMLogTerm logterm,
    const char *rollpath,
    const char *pattern);
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreate(
    const char *name,
    const char *accept_host,
    int listen_port,
    const char *pattern);

CMUTIL_API void CMUTIL_LogFallback(CMLogLevel level,
        const char *file, int line, const char *fmt, ...);

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


CMUTIL_API CMBool CMUTIL_LogIsEnabled(
        CMUTIL_Logger *logger, CMLogLevel level);

#define CMLogIsEnabled(level) CMUTIL_LogIsEnabled(__CMUTIL_GetLogger(),level)

#define CMUTIL_Log__(level,stack,f,...) do {                            \
    CMUTIL_Logger *__logger = __CMUTIL_GetLogger();                     \
    if (__logger)                                                       \
        __logger->LogEx(__logger, CMLogLevel_##level,__FILE__,__LINE__, \
                      CM##stack,f,##__VA_ARGS__);                       \
    else                                                                \
        CMUTIL_LogFallback(CMLogLevel_##level, __FILE__, __LINE__,      \
                           f, ##__VA_ARGS__);                           \
    } while(0)
#define CMUTIL_Log2__(level,stack,f,...)    do {                        \
    CMUTIL_Logger *__logger = __CMUTIL_GetLogger();                     \
    if (__logger)                                                       \
        __logger->LogEx(__logger, level,__FILE__,__LINE__,              \
                      CM##stack,f,##__VA_ARGS__);                       \
    else                                                                \
        CMUTIL_LogFallback(CMLogLevel_##level, __FILE__, __LINE__,      \
                           f, ##__VA_ARGS__);                           \
    } while(0)

#define CMLogTrace(f,...)   CMUTIL_Log__(Trace,False,f,##__VA_ARGS__)
#define CMLogTraceS(f,...)  CMUTIL_Log__(Trace,True ,f,##__VA_ARGS__)

#define CMLogDebug(f,...)   CMUTIL_Log__(Debug,False,f,##__VA_ARGS__)
#define CMLogDebugS(f,...)  CMUTIL_Log__(Debug,True ,f,##__VA_ARGS__)

#define CMLogInfo(f,...)    CMUTIL_Log__(Info ,False,f,##__VA_ARGS__)
#define CMLogInfoS(f,...)   CMUTIL_Log__(Info ,True ,f,##__VA_ARGS__)

#define CMLogWarn(f,...)    CMUTIL_Log__(Warn ,False,f,##__VA_ARGS__)
#define CMLogWarnS(f,...)   CMUTIL_Log__(Warn ,True ,f,##__VA_ARGS__)

#define CMLogError(f,...)   CMUTIL_Log__(Error,False,f,##__VA_ARGS__)
#define CMLogErrorS(f,...)  CMUTIL_Log__(Error,True ,f,##__VA_ARGS__)

#define CMLogFatal(f,...)   CMUTIL_Log__(Fatal,False,f,##__VA_ARGS__)
#define CMLogFatalS(f,...)  CMUTIL_Log__(Fatal,True ,f,##__VA_ARGS__)

#define CMLog(l,f,...)      CMUTIL_Log2__(l,False,f,##__VA_ARGS__)
#define CMLogS(l,f,...)     CMUTIL_Log2__(l,True ,f,##__VA_ARGS__)

typedef struct CMUTIL_LogSystem CMUTIL_LogSystem;
struct CMUTIL_LogSystem {
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
    CMUTIL_Logger *(*GetLogger)(
            const CMUTIL_LogSystem *logsys, const char *name);
    void(*Destroy)(CMUTIL_LogSystem *logsys);
};

CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemCreate(void);
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(
        const char *jsonfile);
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemGet(void);
CMUTIL_API void CMUTIL_LogSystemSet(CMUTIL_LogSystem *lsys);


typedef struct CMUTIL_StackWalker CMUTIL_StackWalker;
struct CMUTIL_StackWalker {
    CMUTIL_StringArray *(*GetStack)(
            const CMUTIL_StackWalker *walker, int skipdepth);
    void(*PrintStack)(
            const CMUTIL_StackWalker *walker,
            CMUTIL_String *outbuf, int skipdepth);
    void(*Destroy)(CMUTIL_StackWalker *walker);
};

CMUTIL_API CMUTIL_StackWalker *CMUTIL_StackWalkerCreate(void);

typedef enum CMSocketResult {
    CMSocketOk = 0,
    CMSocketTimeout,
    CMSocketPollFailed,
    CMSocketReceiveFailed,
    CMSocketSendFailed,
    CMSocketUnsupported,
    CMSocketNotConnected,
    CMSocketConnectFailed,
    CMSocketBindFailed,
    CMSocketUnknownError = 0x7FFFFFFF
} CMSocketResult;

typedef struct sockaddr_in CMUTIL_SocketAddr;
CMUTIL_API CMSocketResult CMUTIL_SocketAddrGet(
        const CMUTIL_SocketAddr *saddr, char *hostbuf, int *port);
CMUTIL_API CMSocketResult CMUTIL_SocketAddrSet(
        CMUTIL_SocketAddr *saddr, const char *host, int port);

typedef struct CMUTIL_Socket CMUTIL_Socket;
struct CMUTIL_Socket {
    CMSocketResult (*Read)(
            const CMUTIL_Socket *socket,
            CMUTIL_String *buffer, uint32_t size, long timeout);
    CMSocketResult (*Write)(
            const CMUTIL_Socket *socket, CMUTIL_String *data, long timeout);
    CMSocketResult (*WritePart)(
            const CMUTIL_Socket *socket, CMUTIL_String *data,
            int offset, uint32_t length, long timeout);
    CMSocketResult (*CheckReadBuffer)(
            const CMUTIL_Socket *socket, long timeout);
    CMSocketResult (*CheckWriteBuffer)(
            const CMUTIL_Socket *socket, long timeout);
    CMUTIL_Socket *(*ReadSocket)(
            const CMUTIL_Socket *socket,
            long timeout, CMSocketResult *rval);
    CMSocketResult (*WriteSocket)(
            const CMUTIL_Socket *socket,
            CMUTIL_Socket *tobesent, pid_t pid, long timeout);
    void (*GetRemoteAddr)(
            const CMUTIL_Socket *socket, CMUTIL_SocketAddr *addr);
    void (*Close)(
            CMUTIL_Socket *socket);
    SOCKET (*GetRawSocket)(
            const CMUTIL_Socket *socket);
    int (*ReadByte)(
            const CMUTIL_Socket *socket);
    CMSocketResult (*WriteByte)(
            const CMUTIL_Socket *socket, uint8_t c);
};

/**
 * Create a new socket which connected to the given endpoint.
 *
 * @param host host where connect to.
 * @param port port where connect to.
 * @param timeout connect timeout in milliseconds.
 * @return A connected socket if succeeded it must be closed after use.
 *      NULL if connect failed.
 */
CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnect(
        const char *host, int port, long timeout);
CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnectWithAddr(
        const CMUTIL_SocketAddr *saddr, long timeout);

CMUTIL_API CMUTIL_Socket *CMUTIL_SSLSocketConnect(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout);
CMUTIL_API CMUTIL_Socket *CMUTIL_SSLSocketConnectWithAddr(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const CMUTIL_SocketAddr *saddr, long timeout);

typedef struct CMUTIL_ServerSocket CMUTIL_ServerSocket;
struct CMUTIL_ServerSocket {
    CMSocketResult (*Accept)(
            const CMUTIL_ServerSocket *server,
            CMUTIL_Socket **res,
            long timeout);
    void (*Close)(CMUTIL_ServerSocket *server);
};

CMUTIL_API CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
        const char *host, int port, int qcnt);

CMUTIL_API CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreate(
        const char *cert, const char *key, const char *ca,
        const char *host, int port, int qcnt);

CMUTIL_API CMBool CMUTIL_SocketPair(
        CMUTIL_Socket **s1, CMUTIL_Socket **s2);


typedef struct CMUTIL_DGramSocket CMUTIL_DGramSocket;
struct CMUTIL_DGramSocket {
    CMSocketResult (*Bind)(
            CMUTIL_DGramSocket *dsock,
            const CMUTIL_SocketAddr *saddr);
    CMSocketResult (*Connect)(
            CMUTIL_DGramSocket *dsock,
            const CMUTIL_SocketAddr *saddr);
    CMBool (*IsConnected)(
            CMUTIL_DGramSocket *dsock);
    void (*Disconnect)(
            CMUTIL_DGramSocket *dsock);
    CMSocketResult (*GetRemoteAddr)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_SocketAddr *saddr);
    CMSocketResult (*GetLocalAddr)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_SocketAddr *saddr);
    CMSocketResult (*Send)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            long timeout);
    CMSocketResult (*Recv)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            long timeout);
    CMSocketResult (*SendTo)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            CMUTIL_SocketAddr *saddr,
            long timeout);
    CMSocketResult (*RecvFrom)(
            CMUTIL_DGramSocket *dsock,
            CMUTIL_ByteBuffer *buf,
            CMUTIL_SocketAddr *saddr,
            long timeout);
    void (*Close)(
            CMUTIL_DGramSocket *dsock);
};

CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreate(void);
CMUTIL_API CMUTIL_DGramSocket *CMUTIL_DGramSocketCreateBind(
        CMUTIL_SocketAddr *addr);


typedef enum CMJsonType {
    CMJsonTypeValue = 0,
    CMJsonTypeObject,
    CMJsonTypeArray
} CMJsonType;

typedef struct CMUTIL_Json CMUTIL_Json;
struct CMUTIL_Json {
    void (*ToString)(
            const CMUTIL_Json *json, CMUTIL_String *buf, CMBool pretty);
    CMJsonType (*GetType)(
            const CMUTIL_Json *json);
    CMUTIL_Json *(*Clone)(
            const CMUTIL_Json *json);
    void (*Destroy)(CMUTIL_Json *json);
};

typedef enum CMJsonValueType {
    CMJsonValueLong = 0,
    CMJsonValueDouble,
    CMJsonValueString,
    CMJsonValueBoolean,
    CMJsonValueNull
} CMJsonValueType;

typedef struct CMUTIL_JsonValue CMUTIL_JsonValue;
struct CMUTIL_JsonValue {
    CMUTIL_Json parent;
    CMJsonValueType (*GetValueType)(
            const CMUTIL_JsonValue *jval);
    int64_t (*GetLong)(
            const CMUTIL_JsonValue *jval);
    double (*GetDouble)(
            const CMUTIL_JsonValue *jval);
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonValue *jval);
    const char *(*GetCString)(
            const CMUTIL_JsonValue *jval);
    CMBool (*GetBoolean)(
            const CMUTIL_JsonValue *jval);
    void (*SetLong)(
            CMUTIL_JsonValue *jval, int64_t inval);
    void (*SetDouble)(
            CMUTIL_JsonValue *jval, double inval);
    void (*SetString)(
            CMUTIL_JsonValue *jval, const char *inval);
    void (*SetBoolean)(
            CMUTIL_JsonValue *jval, CMBool inval);
    void (*SetNull)(
            CMUTIL_JsonValue *jval);
};

CMUTIL_API CMUTIL_JsonValue *CMUTIL_JsonValueCreate(void);

typedef struct CMUTIL_JsonObject CMUTIL_JsonObject;
struct CMUTIL_JsonObject {
    CMUTIL_Json parent;
    CMUTIL_StringArray *(*GetKeys)(
            const CMUTIL_JsonObject *jobj);
    CMUTIL_Json *(*Get)(
            const CMUTIL_JsonObject *jobj, const char *key);
    int64_t (*GetLong)(
            const CMUTIL_JsonObject *jobj, const char *key);
    double (*GetDouble)(
            const CMUTIL_JsonObject *jobj, const char *key);
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonObject *jobj, const char *key);
    const char *(*GetCString)(
            const CMUTIL_JsonObject *jobj, const char *key);
    CMBool (*GetBoolean)(
            const CMUTIL_JsonObject *jobj, const char *key);
    void (*Put)(
            CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Json *json);
    void (*PutLong)(
            CMUTIL_JsonObject *jobj, const char *key, int64_t value);
    void (*PutDouble)(
            CMUTIL_JsonObject *jobj, const char *key, double value);
    void (*PutString)(
            CMUTIL_JsonObject *jobj, const char *key, const char *value);
    void (*PutBoolean)(
            CMUTIL_JsonObject *jobj, const char *key, CMBool value);
    void (*PutNull)(
            CMUTIL_JsonObject *jobj, const char *key);
    CMUTIL_Json *(*Remove)(
            CMUTIL_JsonObject *jobj, const char *key);
    void (*Delete)(
            CMUTIL_JsonObject *jobj, const char *key);
};

CMUTIL_API CMUTIL_JsonObject *CMUTIL_JsonObjectCreate(void);

typedef struct CMUTIL_JsonArray CMUTIL_JsonArray;
struct CMUTIL_JsonArray {
    CMUTIL_Json parent;
    size_t (*GetSize)(
            const CMUTIL_JsonArray *jarr);
    CMUTIL_Json *(*Get)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    int64_t (*GetLong)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    double (*GetDouble)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    const CMUTIL_String *(*GetString)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    const char *(*GetCString)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    CMBool (*GetBoolean)(
            const CMUTIL_JsonArray *jarr, uint32_t index);
    void (*Add)(
            CMUTIL_JsonArray *jarr, CMUTIL_Json *json);
    void (*AddLong)(
            CMUTIL_JsonArray *jarr, int64_t value);
    void (*AddDouble)(
            CMUTIL_JsonArray *jarr, double value);
    void (*AddString)(
            CMUTIL_JsonArray *jarr, const char *value);
    void (*AddBoolean)(
            CMUTIL_JsonArray *jarr, CMBool value);
    void (*AddNull)(
            CMUTIL_JsonArray *jarr);
    CMUTIL_Json *(*Remove)(
            CMUTIL_JsonArray *jarr, uint32_t index);
    void (*Delete)(
            CMUTIL_JsonArray *jarr, uint32_t index);
};

CMUTIL_API CMUTIL_JsonArray *CMUTIL_JsonArrayCreate(void);

CMUTIL_API CMUTIL_Json *CMUTIL_JsonParse(CMUTIL_String *jsonstr);
#define CMUTIL_JsonDestroy(a)   CMCall((CMUTIL_Json*)(a), Destroy)

CMUTIL_API CMUTIL_Json *CMUTIL_XmlToJson(CMUTIL_XmlNode *node);



//////////////////////////////////////////////////////////////////////////////
// NIO Implementations
//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Operation-set bit for socket-accept operations.
 *
 * Suppose that a selection key's interest set contains OP_ACCEPT
 * at the start of a selection operation.
 * If the selector detects that the corresponding server-socket channel is
 * ready to accept another connection, or has an error pending,
 * then it will add OP_ACCEPT to the key's ready set
 * and add the key to its selected-key set.
 */
#define CMUTIL_NIO_OP_ACCEPT    (0x1 << 0)
/**
 * @brief Operation-set bit for socket-connect operations.
 *
 * Suppose that a selection key's interest set contains OP_CONNECT
 * at the start of a selection operation.
 * If the selector detects that the corresponding socket channel is
 * ready to complete its connection sequence, or has an error pending,
 * then it will add OP_CONNECT to the key's ready set
 * and add the key to its selected-key set.
 */
#define CMUTIL_NIO_OP_CONNECT   (0x1 << 1)
/**
 * @brief Operation-set bit for read operations.
 *
 * Suppose that a selection key's interest set contains OP_READ
 * at the start of a selection operation.
 * If the selector detects that the corresponding channel is ready for reading,
 * has reached end-of-stream, has been remotely shut down for further reading,
 * or has an error pending,
 * then it will add OP_READ to the key's ready-operation set
 * and add the key to its selected-key set.
 */
#define CMUTIL_NIO_OP_READ      (0x1 << 2)
/**
 * @brief Operation-set bit for write operations.
 *
 * Suppose that a selection key's interest set contains OP_WRITE
 * at the start of a selection operation.
 * If the selector detects that the corresponding channel is ready for writing,
 * has been remotely shut down for further writing, or has an error pending,
 * then it will add OP_WRITE to the key's ready set
 * and add the key to its selected-key set.
 */
#define CMUTIL_NIO_OP_WRITE     (0x1 << 3)

typedef struct CMUTIL_NIOBuffer CMUTIL_NIOBuffer;
struct CMUTIL_NIOBuffer {
    CMUTIL_NIOBuffer *(*Flip)(
            CMUTIL_NIOBuffer *buffer);
    CMBool (*HasRemaining)(
            const CMUTIL_NIOBuffer *buffer);
    CMUTIL_NIOBuffer *(*Clear)(
            CMUTIL_NIOBuffer *buffer);
    int (*Capacity)(
            const CMUTIL_NIOBuffer *buffer);
    CMUTIL_NIOBuffer *(*Put)(
            CMUTIL_NIOBuffer *buffer, int data);
    CMUTIL_NIOBuffer *(*PutBytes)(
            CMUTIL_NIOBuffer *buffer, const uint8_t *data,
            int length);
    CMUTIL_NIOBuffer *(*PutBytesPart)(
            CMUTIL_NIOBuffer *buffer, const uint8_t *data,
            int offset, int length);
    int (*Get)(
            CMUTIL_NIOBuffer *buffer);
    int (*GetAt)(
            CMUTIL_NIOBuffer *buffer, int index);
    CMUTIL_NIOBuffer *(*GetBytes)(
            CMUTIL_NIOBuffer *buffer, uint8_t *dest, int length);
    CMUTIL_NIOBuffer *(*GetBytesPart)(
            CMUTIL_NIOBuffer *buffer, uint8_t *dest, int offset, int length);
    void (*Compact)(
            CMUTIL_NIOBuffer *buffer);
    CMUTIL_NIOBuffer *(*Mark)(
            CMUTIL_NIOBuffer *buffer);
    CMUTIL_NIOBuffer *(*Reset)(
            CMUTIL_NIOBuffer *buffer);
    CMUTIL_NIOBuffer *(*Rewind)(
            CMUTIL_NIOBuffer *buffer);
    int (*Limit)(
            const CMUTIL_NIOBuffer *buffer);
    int (*Position)(
            const CMUTIL_NIOBuffer *buffer);
    CMBool (*Equals)(
            const CMUTIL_NIOBuffer *buffer, const CMUTIL_NIOBuffer *buffer2);
    int (*CompareTo)(
            const CMUTIL_NIOBuffer *buffer, const CMUTIL_NIOBuffer *buffer2);
    void (*Destroy)(
            CMUTIL_NIOBuffer *buffer);
};

CMUTIL_API CMUTIL_NIOBuffer *CMUTIL_NIOBufferCreate(int capacity);

typedef struct CMUTIL_NIOChannel CMUTIL_NIOChannel;
typedef struct CMUTIL_NIOSelector CMUTIL_NIOSelector;
typedef struct CMUTIL_NIOSelectionKey CMUTIL_NIOSelectionKey;

struct CMUTIL_NIOChannel {
    void (*Close)(
            CMUTIL_NIOChannel *channel);
    CMBool (*IsOpen)(
            const CMUTIL_NIOChannel *channel);
    CMBool (*IsRegistered)(
            const CMUTIL_NIOChannel *channel);
    CMUTIL_NIOSelectionKey *(*KeyFor)(
            const CMUTIL_NIOChannel *channel, CMUTIL_NIOSelector *selector);
    CMUTIL_NIOSelectionKey *(*Register)(
            CMUTIL_NIOChannel *channel, CMUTIL_NIOSelector *selector, int ops);
    int (*ValidOps)(
            const CMUTIL_NIOChannel *channel);
};

struct CMUTIL_NIOSelectionKey {
    CMUTIL_NIOChannel *(*Attach)(
            CMUTIL_NIOSelectionKey *selkey, CMUTIL_NIOChannel *channel);
    CMUTIL_NIOChannel *(*Attachment)(
            CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Retrieves this key's interest set.
     *
     * It is guaranteed that the returned set will only contain operation bits
     * that are valid for this key's channel.
     *
     * This method may be invoked at any time.
     * Whether or not it blocks, and for how long, is implementation-dependent.
     *
     * @return This key's interest set
     */
    int (*InterestOps)(
            const CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Sets this key's interest set to the given value.
     *
     * This method may be invoked at any time.
     * Whether or not it blocks, and for how long, is implementation-dependent.
     *
     * @param ops The new interest set
     * @return This selection key
     */
    CMUTIL_NIOSelectionKey *(*SetInterestOps)(
            CMUTIL_NIOSelectionKey *selkey, int ops);
    CMBool (*IsAcceptible)(
            const CMUTIL_NIOSelectionKey *selkey);
    CMBool (*IsConnectable)(
            const CMUTIL_NIOSelectionKey *selkey);
    CMBool (*IsReadable)(
            const CMUTIL_NIOSelectionKey *selkey);
    CMBool (*IsWritable)(
            const CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Tells whether this key is valid.
     *
     * A key is valid upon creation and remains so until it is canceled,
     * its channel is closed, or its selector is closed.
     *
     * @return CMTrue if, and only if, this key is valid
     */
    CMBool (*IsValid)(
            const CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Retrieves this key's ready-operation set.
     *
     * It is guaranteed that the returned set will only contain operation bits
     * that are valid for this key's channel.
     *
     * @return This key's ready-operation set
     */
    int (*ReadyOps)(
            const CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Returns the selector for which this key was created.
     *
     * This method will continue to return the selector even after
     * the key is cancelled.
     */
    CMUTIL_NIOSelector *(*Selector)(
            CMUTIL_NIOSelectionKey *selkey);
    /**
     * @brief Requests that the registration of this key's channel with
     * its selector be canceled.
     *
     * Upon return the key will be invalid and will have been added to its
     * selector's canceled-key set.
     * The key will be removed from all the selector's key sets during
     * the next selection operation.
     *
     * If this key has already been canceled then
     * invoking this method unpredictable.
     *
     * This method may be invoked at any time.
     * It synchronizes on the selector's canceled-key set,
     * and therefore may block briefly if invoked concurrently
     * with a cancellation or selection operation involving the same selector.
     */
    void (*Cancel)(
            CMUTIL_NIOSelectionKey *selkey);
};

struct CMUTIL_NIOSelector {
    void (*Close)(
            CMUTIL_NIOSelector *selector);
    CMBool (*IsOpen)(
            const CMUTIL_NIOSelector *selector);
    CMUTIL_Array *(*Keys)(
            CMUTIL_NIOSelector *selector);
    int (*Select)(
            CMUTIL_NIOSelector *selector);
    int (*SelectTimeout)(
            CMUTIL_NIOSelector *selector, long timeout);
    CMUTIL_Array *(*SelectedKeys)(
            CMUTIL_NIOSelector *selector);
    CMUTIL_NIOSelector *(*WakeUp)(
            CMUTIL_NIOSelector *selector);
};

#ifdef __cplusplus
}
#endif

#endif // LIBCMUTILS_H__

