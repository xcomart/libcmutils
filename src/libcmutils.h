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
# define strcasecmp _stricmp
# define stat       _stat
# define pid_t      DWORD
#endif

/**
 * @defgroup CMUTIL Types.
 * @{
 *
 * CMUTIL library uses platform independent data types for multi-platform
 * support.
 */

/**
 * 64 bit signed / unsigned integer max values.
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
 * 64 bit signed / unsigned integer printf format string.
 */
#if !defined(PRINT64I)
# define PRINT64I  "%"PRIi64
# define PRINT64U  "%"PRIu64
#endif

/**
 * @brief Boolean definition for this library.
 */
typedef enum CMBool {
    CMTrue         =0x0001,
    CMFalse        =0x0000
} CMBool;

/**
 * @}
 */

#if defined(APPLE)
/**
 * For Mac OS X.
 * Redefine gethostbyname for consistancy of gethostbyname_r implementation.
 */
# define gethostbyname      CMUTIL_NetworkGetHostByName
# define gethostbyname_r    CMUTIL_NetworkGetHostByNameR
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
 * This library built on C language, but some of the usage of this library
 * simillar to object oriented languages like C++ or Java.
 *
 * Create instance of type CMUTIL_XX with CMUTIL_XXCreate function,
 * and call method with member callbacks.
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
 * As you see instance variable used redundently in method call,
 * we created an macro CMCall because this inconvinience.
 * As a result below code will produce the same result with above code.
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
 * @defgroup CMUTILS_Initialization Initialization and memory operations.
 * @{
 *
 * libcmutils offers three types of memory management:
 *
 * <ul>
 *  <li> System version malloc, realloc, strdup and free.
 *      This option has no memory management, proper for external memory
 *      debugger like 'duma' or 'valgrind'.</li>
 *  <li> Memory recycling, all memory allocation will be fit to 2^n sized memory
 *      blocks. This option prevents heap memory fragments,
 *      supports memory leak detection and detects memory overflow/underflow
 *      corruption while freeing.</li>
 *  <li> Memory recycling with stack information.
 *      This option is same as previous recycling option except all memory
 *      allocation will also create it's callstack information,
 *      increadibly slow. Only use for memory leak debugging.</li>
 * </ul>
 */

/**
 * @typedef CMUTIL_MemOper Memory operation types.
 * Refer CMUTIL_Init function for details.
 */
typedef enum CMUTIL_MemOper {
    /**
     * Use system version malloc, realloc, strdup and free.
     */
    CMUTIL_MemSystem = 0,
    /**
     * Use memory recycle technique.
     */
    CMUTIL_MemRecycle,
    /**
     * Use memory operation with debugging informations.
     */
    CMUTIL_MemDebug
} CMUTIL_MemOper;

/**
 * @brief Initialize this library. This function must be called before calling
 * any function in this library.
 *
 *  <ul>
 *   <li>CMUTIL_MemSystem: All memory operation will be replaced with
 *         malloc, realloc, strdup and free.
 *         same as direct call system calls.</li>
 *   <li>CMUTIL_MemRecycle: All memory allocation will be managed with pool.
 *         Pool is consist of memory blocks which size is power of 2 bytes.
 *         Recommended for any purpose. This option prevents heap memory
 *         fragments and checks memory leak at termination. But a little
 *         overhead required for recycling and needs much more memory
 *         usage.</li>
 *   <li>CMUTIL_MemDebug: Option for memory debugging.
 *         Any memory operation will be traced including stack tracing.
 *         Recommended only for memory debugging.
 *         Much slower than any other options.</li>
 *  </ul>
 *
 * @param memoper  Memory operation initializer. Must be one of
 *  CMUTIL_MemRelease, CMUTIL_MemDebug and CMUTIL_MemRecycle.
 *
 */
CMUTIL_API void CMUTIL_Init(
        CMUTIL_MemOper memoper);

/**
 * @brief Clear allocated resource for this library.
 */
CMUTIL_API void CMUTIL_Clear(void);

/**
 * Memory operation interface.
 */
typedef struct CMUTIL_Mem {
    /**
     * @brief Allocate memory.
     *
     * Allocates <code>size<code> bytes and returns a pointer to the allocated
     * memory. The memory is not initialized. If size is 0, then this function
     * returns either NULL, or a unique pointer value that can later be
     * successfully passed to <code>Free</code>.
     *
     * @param size  Count of bytes to be allocated.
     * @return A pointer to the allocated memory, which is suitably aligned for
     *  any built-in type. On error this function returns NULL. NULL may also
     *  be returned by a successfull call to this function with a
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
 * @brief Reallocate memory with given size preserving previous data.
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
 * @brief Platform independent condition definition for concurrency control.
 *
 * Condition(or Event)
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
     * @brief Sets this conidtion object.
     *
     * The state of a manual-reset condition object remains set until it is
     * explicitly to the nonsignaled state by the Reset method. Any number
     * of waiting threads, or threads that subsequently begin wait operations
     * for this condition object by calling one of wait functions, can be
     * released while this condition state is signaled.
     *
     * The state of an auto-reset condition object remains signaled until a
     * single waiting thread is released, at which time the system
     * automatically resets this condition state to nonsignaled. If no
     * threads are waiting, this condition object's state remains signaled.
     *
     * If this condition object set already, calling this method will has no
     * effect.
     * @param cond this condition object.
     */
    void (*Set)(
            CMUTIL_Cond *cond);

    /**
     * @brief Unsets this condition object.
     *
     * The state of this condition object remains nonsignaled until it is
     * explicitly set to signaled by the Set method. This nonsignaled state
     * blocks the execution of any threads that have specified this condition
     * object in a call to one of wait methods.
     *
     * This Reset method is used primarily for manual-reset condition object,
     * which must be set explicitly to the nonsignaled state. Auto-reset
     * condition objects automatically change from signaled to nonsignaled
     * after a single waiting thread is released.
     * @param cond this condition object.
     */
    void (*Reset)(
            CMUTIL_Cond *cond);

    /**
     * @brief Destroys all resources related with this object.
     * @param cond this condition object.
     */
    void (*Destroy)(
            CMUTIL_Cond *cond);
};

/**
 * @brief Creates a condition object.
 *
 * Creates a manual or auto resetting condition object.
 *
 * @param manual_reset
 *     If this parameter is CMTrue, the function creates a
 *     manual-reset condition object, which requires the use of the Reset
 *     method to set the event state to nonsignaled. If this parameter is
 *     CMFalse, the function creates an auto-reset condition
 *     object, and system automatically resets the event state to
 *     nonsignaled after a single waiting thread has been released.
 * @return Created conditional object.
 */
CMUTIL_API CMUTIL_Cond *CMUTIL_CondCreate(CMBool manual_reset);

/**
 * @brief Platform independent mutex implementation.
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
     * This mutex is recursive lockable object. This mutex shall maintain the
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
     * This mutex object shall be unlocked if lock count reaches zero.
     * @param mutex This mutex object.
     */
    void (*Unlock)(CMUTIL_Mutex *mutex);

    /**
     * @brief Try to lock given mutex object.
     * @param  mutex   a mutex object to be tested.
     * @return CMTrue if mutex locked successfully,
     *         CMFalse if lock failed.
     */
    CMBool (*TryLock)(CMUTIL_Mutex *mutex);

    /**
     * @brief Destroys all resources related with this object.
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
 * @return Created mutex object.
 */
CMUTIL_API CMUTIL_Mutex *CMUTIL_MutexCreate(void);


/**
 * @brief Platform independent thread object.
 */
typedef struct CMUTIL_Thread CMUTIL_Thread;
struct CMUTIL_Thread {

    /**
     * @brief Starts this thread.
     *
     * This function creates a new thread which is not detached,
     * so the created thread must be joined by calling Join method.
     * @param thread This thread object.
     * @return This thread have been started successfully or not.
     */
    CMBool (*Start)(CMUTIL_Thread *thread);

    /**
     * @brief Join this thread.
     *
     * This function joins this thread and clean up it's resources including
     * this thread object, so this references are must not be used after
     * calling this method. Every threads which been created by this library
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
     *     just internal thread index.
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
 * Call <tt>Start</tt> method to start thread.
 * Returned object must be free by calling <tt>Join</tt> method,
 * even if thread not started.
 *
 * @param proc Start routine of the created thread.
 * @param udata This argument is passed as the sole argument of 'proc'
 * @param name Thread name, any name could be assigned,
 *      and can also duplicable. But must not be exceed 200 bytes.
 * @return Created thread object.
 */
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadCreate(
        void*(*proc)(void*), void *udata, const char *name);

/**
 * @brief Get the ID of current thread. This id is not system thread id,
 *     just internal thread index.
 * @return ID of current thread.
 */
CMUTIL_API uint32_t CMUTIL_ThreadSelfId(void);

/**
 * @brief Get current thread context.
 * @return Current thread context.
 */
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadSelf(void);

/**
 * @brief Get system dependent thread id.
 * @return System dependent thread id.
 */
CMUTIL_API uint64_t CMUTIL_ThreadSystemSelfId(void);

/**
 * @brief Platform independent semaphore object.
 */
typedef struct CMUTIL_Semaphore CMUTIL_Semaphore;
struct CMUTIL_Semaphore {

    /**
     * @brief Acquire an ownership from semaphore.
     *
     * Acquire an ownership in given time or fail with timed out. This method
     * will blocked until an ownership acquired successfully in time,
     * or the waiting time expired.
     *
     * @param semaphore This semaphore object.
     * @param millisec Waiting time to acquire an ownership in millisecond.
     * @return CMTrue if an ownership acquired successfully in time,
     *         CMFalse if failed to acquire ownership in time.
     */
    CMBool (*Acquire)(
            CMUTIL_Semaphore *semaphore, long millisec);

    /**
     * @brief Release an ownership to semaphore.
     *
     * Release an ownership to semaphore, the semaphore value increased
     * greater than zero consequently, one of the thread which blocked in
     * Acquire method will get the ownership and be unblocked.
     * @param semaphore This semaphore object.
     */
    void (*Release)(
            CMUTIL_Semaphore *semaphore);

    /**
     * @brief Destroy all resources related with this object.
     * @param semaphore This semaphore object.
     */
    void (*Destroy)(CMUTIL_Semaphore *semaphore);
};

/**
 * @brief Creates a semaphore object.
 *
 * Create an in-process semaphore object.
 * @param initcnt Initial semaphore ownership count.
 * @return Created semaphore object.
 */
CMUTIL_API CMUTIL_Semaphore *CMUTIL_SemaphoreCreate(int initcnt);


/**
 * @brief Platform independent read/write lock object.
 */
typedef struct CMUTIL_RWLock CMUTIL_RWLock;
struct CMUTIL_RWLock {

    /**
     * @brief Read lock for this object.
     *
     * This method shall apply a read lock to given object. The calling thread
     * acquires the read lock if a writer does not hold the lock and
     * there are no writers blocked on the lock.
     * @param rwlock This read-write lock object.
     */
    void (*ReadLock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Read unlock for this object.
     *
     * This method shall release a read lock held on given object.
     * @param rwlock This read-write lock object.
     */
    void (*ReadUnlock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Write lock for this object.
     *
     * This method whall apply a write lock to given object. The calling thread
     * acquires the write lock if no other thread (reader or writer) holds
     * given object. Otherwise, the thread shall block until it can acquire
     * the lock. The calling thread may deadlock if at the time
     * the call is made it holds the read-write lock
     * (whether a read or write lock).
     * @param rwlock This read-write lock object.
     */
    void (*WriteLock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Write unlock for this object.
     *
     * This method shall release a write lock held on given object.
     * @param rwlock This read-write lock object.
     */
    void (*WriteUnlock)(
            CMUTIL_RWLock *rwlock);

    /**
     * @brief Destroy all resources related with this object.
     * @param rwlock This read-write lock object.
     */
    void (*Destroy)(CMUTIL_RWLock *rwlock);
};

/**
 * @brief Create a read-write lock object.
 */
CMUTIL_API CMUTIL_RWLock *CMUTIL_RWLockCreate(void);

/**
 * @brief Iterator of collection members.
 */
typedef struct CMUTIL_Iterator CMUTIL_Iterator;
struct CMUTIL_Iterator {

    /**
     * @brief Check iterator has next element.
     * @param iter This iterator object.
     * @return CMTrue if this iterator has next element.
     *         CMFalse if there is no more elements.
     */
    CMBool (*HasNext)(
            const CMUTIL_Iterator *iter);

    /**
     * @brief Get next element from this iterator.
     * @param iter This iterator object.
     * @return Next element.
     */
    void *(*Next)(
            CMUTIL_Iterator *iter);

    /**
     * @brief Destroy this iterator.
     *
     * This method must be called after use this iterator.
     * @param iter This iterator object.
     */
    void (*Destroy)(
            CMUTIL_Iterator *iter);
};


/**
 * @brief Dynamic array of any type element.
 */
typedef struct CMUTIL_Array CMUTIL_Array;
struct CMUTIL_Array {

    /**
     * @brief Add new element to this array.
     *
     * If this array is a sorted array(created including <code>comparator</code>
     * callback function), this method will add new item to appropriate
     * location.
     * Otherwise this method will add new item to the end of this array.
     *
     * @param array This dynamic array object.
     * @param item Item to be added.
     * @return Previous data at that position if this array is sorted array.
     *         NULL if this is not sorted array.
     */
    void *(*Add)(CMUTIL_Array *array, void *item);

    /**
     * @brief Remove an item from sorted array.
     *
     * This method will only work if this array is a sorted array.
     * <code>compval</code> will be compared with array elements in
     * binary search manner.
     *
     * @param array This dynamic array object.
     * @param compval Search key of item to be removed.
     * @return Removed element if given item found in this array.
     *         NULL if given key does not exists in this array or
     *         this array is not a sorted array.
     */
    void *(*Remove)(CMUTIL_Array *array, const void *compval);

    /**
     * @brief Insert a new element to this array.
     *
     * This method will only work if this array is not a sorted array.
     * New item will be stored at given <code>index</code>.
     *
     * @param array This dynamic array object.
     * @param item Item to be inserted.
     * @param index The index of this array where new item will be stored in.
     * @return Inserted item if this array is not a sorted array.
     *         NULL if this array is a sorted array.
     */
    void *(*InsertAt)(CMUTIL_Array *array, void *item, uint32_t index);

    /**
     * @brief Remove an item from this array.
     *
     * The item at the index of this array will be removed.
     *
     * @param array This dynamic array object.
     * @param index The index of this array which item will be removed.
     * @return Removed item if given index is a valid index.
     *         NULL if given index is invalid.
     */
    void *(*RemoveAt)(CMUTIL_Array *array, uint32_t index);

    /**
     * @brief Replace an item at the position of this array.
     *
     * This method will only work if this array is not a sorted array.
     * The item positioned at the <code>index</code> of this array will be
     * replaced with given <code>item</code>.
     *
     * @param array This dynamic array object.
     * @param item New item which will replace old one.
     * @param index The index of this array, the item in which will be replaced.
     * @return Replaced old item if this array is not a sorted array and
     *         given index is a valid index.
     *         NULL if this array is a sorted array or given index is not a
     *         valid index or the old item is NULL.
     */
    void *(*SetAt)(CMUTIL_Array *array, void *item, uint32_t index);

    /**
     * @brief Get an item from this array.
     *
     * Get an item which stored at given <code>index</code>.
     *
     * @param array This dynamic array object.
     * @param index The index of this array,
     *        the item in which will be retreived.
     * @return An item which positioned at <code>index</code> of this array if
     *         given index is valid one.
     *         NULL if given index is invalid.
     */
    void *(*GetAt)(const CMUTIL_Array *array, uint32_t index);

    /**
     * @brief Find an item from this array.
     *
     * This method will only work if this array is a sorted array.
     *
     * @param array This dynamic array object.
     * @param compval Search key of item which to be found.
     * @param index The index reference of the item which found with
     *        <code>compval</code>.
     *        Where the found item index will be stored in.
     * @return A found item from this array if the item found. NULL if
     *         this array is not a sorted array or item not found.
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
     * @brief Push an item to this array like stack operation.
     *
     * This method will only work if this array is not a sorted array.
     * This operation will add given <code>item</code> at the end of this array.
     *
     * @param array This dynamic array object.
     * @param item A new item to be pushed to this array.
     * @return CMTrue if push operation performed successfully.
     *         CMFalse otherwise.
     */
    CMBool (*Push)(CMUTIL_Array *array, void *item);

    /**
     * @brief Pop an item from this array like stack operation.
     *
     * This operation will remove an item at the end of this array.
     *
     * @param array This dynamic array object.
     * @return Removed item if there are elements exists. NULL if there is
     *         no more elements.
     */
    void *(*Pop)(CMUTIL_Array *array);

    /**
     * @brief Get the top element from this array like stack operation.
     *
     * Get the item at the end of this array.
     *
     * @param array This dynamic array object.
     * @return An item at the end of this array
     *         if the size of this array is bigger than zero.
     *         NULL if there is no item in this array.
     */
    void *(*Top)(const CMUTIL_Array *array);

    /**
     * @brief Get the bottom element from this array like stack operation.
     *
     * Get the item at beginning of this array.
     *
     * @param array This dynamic array object.
     * @return An item at the beginning of this array
     *         if the size of this array is bigger than zero.
     *         NULL if there is no item in this array.
     */
    void *(*Bottom)(const CMUTIL_Array *array);

    /**
     * @brief Get iterator of all item in this array.
     *
     * @param array This dynamic array object.
     * @return An {@link CMUTIL_Iterator CMUTIL_Iterator} object of
     *         all item in this array.
     * @see {@link CMUTIL_Iterator CMUTIL_Iterator}
     */
    CMUTIL_Iterator *(*Iterator)(const CMUTIL_Array *array);

    /**
     * @brief Clear this array object.
     *
     * Clear all item in this array object.
     * If <code>freecb</code> parameter is supplied, this callback will be
     * called to all items in this array.
     *
     * @param array This dynamic array object.
     */
    void (*Clear)(CMUTIL_Array *array);

    /**
     * @brief Destroy this array object.
     *
     * Destory this object and it's internal allocations.
     * If <code>freecb</code> parameter is supplied, this callback will be
     * called to all items in this array.
     *
     * @param array This dynamic array object.
     */
    void (*Destroy)(CMUTIL_Array *array);
};

/**
 * Default initial capacity of dynamic array.
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
 * @brief CMUTIL_ArrayCreateEx
 * @param initcapacity
 * @return
 */
CMUTIL_API CMUTIL_Array *CMUTIL_ArrayCreateEx(
        size_t initcapacity,
        int (*comparator)(const void*,const void*),
        void(*freecb)(void*));


typedef struct CMUTIL_String CMUTIL_String;
struct CMUTIL_String {
    size_t (*AddString)(
            CMUTIL_String *string, const char *tobeadded);
    size_t (*AddNString)(
            CMUTIL_String *string, const char *tobeadded, size_t size);
    size_t (*AddChar)(
            CMUTIL_String *string, char tobeadded);
    size_t (*AddPrint)(
            CMUTIL_String *string, const char *fmt, ...);
    size_t (*AddVPrint)(
            CMUTIL_String *string, const char *fmt, va_list args);
    size_t (*AddAnother)(
            CMUTIL_String *string, CMUTIL_String *tobeadded);
    size_t (*InsertString)(
            CMUTIL_String *string, const char *tobeadded, uint32_t at);
    size_t (*InsertNString)(
            CMUTIL_String *string,
            const char *tobeadded, uint32_t at, size_t size);
    size_t (*InsertPrint)(
            CMUTIL_String *string, uint32_t idx, const char *fmt, ...);
    size_t (*InsertVPrint)(
            CMUTIL_String *string, uint32_t idx,
            const char *fmt, va_list args);
    size_t (*InsertAnother)(
            CMUTIL_String *string, uint32_t idx, CMUTIL_String *tobeadded);
    void (*CutTailOff)(
            CMUTIL_String *string, size_t length);
    CMUTIL_String *(*Substring)(
            const CMUTIL_String *string, uint32_t offset, size_t length);
    CMUTIL_String *(*ToLower)(
            const CMUTIL_String *string);
    void (*SelfToLower)(
            CMUTIL_String *string);
    CMUTIL_String *(*ToUpper)(
            const CMUTIL_String *string);
    void (*SelfToUpper)(
            CMUTIL_String *string);
    CMUTIL_String *(*Replace)(
            const CMUTIL_String *string,
            const char *needle, const char *alter);
    size_t (*GetSize)(
            const CMUTIL_String *string);
    const char *(*GetCString)(
            const CMUTIL_String *string);
    void (*Clear)(
            CMUTIL_String *string);
    CMUTIL_String *(*Clone)(
            const CMUTIL_String *string);
    void (*Destroy)(
            CMUTIL_String *string);
};

#define CMUTIL_STRING_DEFAULT   32
#define CMUTIL_StringCreate()   \
        CMUTIL_StringCreateEx(CMUTIL_STRING_DEFAULT, NULL)
CMUTIL_API CMUTIL_String *CMUTIL_StringCreateEx(
        size_t initcapacity,
        const char *initcontent);


typedef struct CMUTIL_StringArray CMUTIL_StringArray;
struct CMUTIL_StringArray {
    void (*Add)(
            CMUTIL_StringArray *array, CMUTIL_String *string);
    void (*AddCString)(
            CMUTIL_StringArray *array, const char *string);
    void (*InsertAt)(
            CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index);
    void (*InsertAtCString)(
            CMUTIL_StringArray *array, const char *string, uint32_t index);
    CMUTIL_String *(*RemoveAt)(
            CMUTIL_StringArray *array, uint32_t index);
    CMUTIL_String *(*SetAt)(
            CMUTIL_StringArray *array, CMUTIL_String *string, uint32_t index);
    CMUTIL_String *(*SetAtCString)(
            CMUTIL_StringArray *array, const char *string, uint32_t index);
    CMUTIL_String *(*GetAt)(
            const CMUTIL_StringArray *array, uint32_t index);
    const char *(*GetCString)(
            const CMUTIL_StringArray *array, uint32_t index);
    size_t (*GetSize)(
            const CMUTIL_StringArray *array);
    CMUTIL_Iterator *(*Iterator)(
            const CMUTIL_StringArray *array);
    void (*Destroy)(
            CMUTIL_StringArray *array);
};

#define CMUTIL_STRINGARRAY_DEFAULT	32
#define CMUTIL_StringArrayCreate()	\
        CMUTIL_StringArrayCreateEx(CMUTIL_STRINGARRAY_DEFAULT)
CMUTIL_API CMUTIL_StringArray *CMUTIL_StringArrayCreateEx(
        size_t initcapacity);

CMUTIL_API char *CMUTIL_StrRTrim(char *inp);
CMUTIL_API char *CMUTIL_StrLTrim(char *inp);
CMUTIL_API char *CMUTIL_StrTrim(char *inp);
CMUTIL_API const char *CMUTIL_StrNextToken(
    char *dest, size_t buflen, const char *src, const char *delims);
CMUTIL_API char *CMUTIL_StrSkipSpaces(char *line, const char *spaces);

CMUTIL_API CMUTIL_StringArray *CMUTIL_StringSplit(
    const char *haystack, const char *needle);
CMUTIL_API int CMUTIL_StringHexToBytes(char *dest, const char *src, int len);

typedef struct CMUTIL_Map CMUTIL_Map;
struct CMUTIL_Map {
    void *(*Put)(CMUTIL_Map *map, const char* key, void* value);
    void (*PutAll)(CMUTIL_Map *map, const CMUTIL_Map *src);
    void *(*Get)(const CMUTIL_Map *map, const char* key);
    void *(*Remove)(CMUTIL_Map *map, const char* key);
    CMUTIL_StringArray *(*GetKeys)(const CMUTIL_Map *map);
    size_t (*GetSize)(const CMUTIL_Map *map);
    CMUTIL_Iterator *(*Iterator)(const CMUTIL_Map *map);
    void (*Clear)(CMUTIL_Map *map);
    void (*ClearLink)(CMUTIL_Map *map);
    void (*Destroy)(CMUTIL_Map *map);
    void (*PrintTo)(const CMUTIL_Map *map, CMUTIL_String *out);
};

#define CMUTIL_MAP_DEFAULT	256
#define CMUTIL_MapCreate()	CMUTIL_MapCreateEx(\
        CMUTIL_MAP_DEFAULT, CMFalse, NULL)
CMUTIL_API CMUTIL_Map *CMUTIL_MapCreateEx(
        uint32_t bucketsize, CMBool isucase, void(*freecb)(void*));


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
CMUTIL_API CMUTIL_List *CMUTIL_ListCreateEx(void(*freecb)(void*));





typedef enum CMUTIL_XmlNodeKind {
    CMUTIL_XmlNodeUnknown = 0,
    CMUTIL_XmlNodeText,
    CMUTIL_XmlNodeTag
} CMUTIL_XmlNodeKind;
#define CMUTIL_XmlNodeKind		CMUTIL_XmlNodeKind

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
    CMUTIL_XmlNodeKind (*GetType)(const CMUTIL_XmlNode *node);
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
        CMUTIL_XmlNodeKind type, const char *tagname);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
        CMUTIL_XmlNodeKind type, const char *tagname, size_t len);




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
            void (*proc)(void *param), void *param);
    CMUTIL_TimerTask *(*ScheduleDelay)(
            CMUTIL_Timer *timer, long delay,
            void (*proc)(void *param), void *param);
    CMUTIL_TimerTask *(*ScheduleAtRepeat)(
            CMUTIL_Timer *timer, struct timeval *first, long period,
            void (*proc)(void *param), void *param);
    CMUTIL_TimerTask *(*ScheduleDelayRepeat)(
            CMUTIL_Timer *timer, long delay, long period,
            void (*proc)(void *param), void *param);
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

CMUTIL_API CMUTIL_Pool *CMUTIL_PoolCreate(
        int initcnt,
        int maxcnt,
        void *(*createproc)(void *udata),
        void (*destroyproc)(void *resource, void *udata),
        CMBool (*testproc)(void *resource, void *udata),
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

CMBool CMUTIL_PathCreate(const char *path, uint32_t mode);

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


typedef enum CMUTIL_LogLevel {
    CMUTIL_LogLevel_Trace = 0,
    CMUTIL_LogLevel_Debug,
    CMUTIL_LogLevel_Info,
    CMUTIL_LogLevel_Warn,
    CMUTIL_LogLevel_Error,
    CMUTIL_LogLevel_Fatal
} CMUTIL_LogLevel;

typedef enum CMUTIL_LogTerm {
    CMUTIL_LogTerm_Year = 0,
    CMUTIL_LogTerm_Month,
    CMUTIL_LogTerm_Date,
    CMUTIL_LogTerm_Hour,
    CMUTIL_LogTerm_Minute
} CMUTIL_LogTerm;

typedef struct CMUTIL_LogAppender CMUTIL_LogAppender;

typedef struct CMUTIL_ConfLogger CMUTIL_ConfLogger;
struct CMUTIL_ConfLogger {
    void(*AddAppender)(
        const CMUTIL_ConfLogger *logger,
        CMUTIL_LogAppender *appender,
        CMUTIL_LogLevel level);
};

typedef struct CMUTIL_Logger CMUTIL_Logger;
struct CMUTIL_Logger {
    void(*LogEx)(
        CMUTIL_Logger *logger,
        CMUTIL_LogLevel level,
        const char *file,
        int line,
        CMBool printStack,
        const char *fmt,
        ...);
};

struct CMUTIL_LogAppender {
    const char *(*GetName)(
            const CMUTIL_LogAppender *appender);
    void(*SetAsync)(
            CMUTIL_LogAppender *appender, size_t buffersz);
    void(*Append)(
        CMUTIL_LogAppender *appender,
        CMUTIL_Logger *logger,
        CMUTIL_LogLevel level,
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
    CMUTIL_LogTerm logterm,
    const char *rollpath,
    const char *pattern);
CMUTIL_API CMUTIL_LogAppender *CMUTIL_LogSocketAppenderCreate(
    const char *name,
    const char *accept_host,
    int listen_port,
    const char *pattern);

#define CMUTIL_LogDefine(name)                                          \
    static CMUTIL_Logger *___logger = NULL;                             \
    static CMUTIL_Logger *__CMUTIL_GetLogger() {                        \
        if (___logger == NULL) {                                        \
            CMUTIL_LogSystem *lsys = CMUTIL_LogSystemGet();             \
            if (lsys) ___logger =                                       \
                CMCall(CMUTIL_LogSystemGet(), GetLogger, name);    \
        }                                                               \
        return ___logger;                                               \
    }

#define CMUTIL_Log__(level,stack,f,...) do {                            \
    CMUTIL_Logger *logger = __CMUTIL_GetLogger();                       \
    if (logger)                                                         \
        logger->LogEx(logger, CMUTIL_LogLevel_##level,__FILE__,__LINE__,\
                      CM##stack,f,##__VA_ARGS__);                  \
    } while(0)
#define CMUTIL_Log2__(level,stack,f,...)	do {                        \
    CMUTIL_Logger *logger = __CMUTIL_GetLogger();                       \
    if (logger)                                                         \
        logger->LogEx(logger, level,__FILE__,__LINE__,                  \
                      CMUTIL_##stack,f,##__VA_ARGS__);                  \
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
    CMUTIL_ConfLogger *(*CreateLogger)(
        const CMUTIL_LogSystem *logsys,
        const char *name,
        CMUTIL_LogLevel level,
        CMBool additivity);
    CMUTIL_Logger *(*GetLogger)(
            const CMUTIL_LogSystem *logsys, const char *name);
    void(*Destroy)(CMUTIL_LogSystem *logsys);
};

CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemCreate(void);
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(
        const char *jsonfile);
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemGet(void);


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

typedef enum CMUTIL_SocketResult {
    CMUTIL_SocketOk = 0,
    CMUTIL_SocketTimeout,
    CMUTIL_SocketSelectFailed,
    CMUTIL_SocketReceiveFailed,
    CMUTIL_SocketSendFailed,
    CMUTIL_SocketUnsupported,
    CMUTIL_SocketUnknownError = 0x7FFFFFFF
} CMUTIL_SocketResult;

typedef struct CMUTIL_Socket CMUTIL_Socket;
struct CMUTIL_Socket {
    CMUTIL_SocketResult (*Read)(
            const CMUTIL_Socket *socket,
            CMUTIL_String *buffer, uint32_t size, long timeout);
    CMUTIL_SocketResult (*Write)(
            const CMUTIL_Socket *socket, CMUTIL_String *data, long timeout);
    CMUTIL_SocketResult (*WritePart)(
            const CMUTIL_Socket *socket, CMUTIL_String *data,
            int offset, uint32_t length, long timeout);
    CMUTIL_SocketResult (*CheckReadBuffer)(
            const CMUTIL_Socket *sock, long timeout);
    CMUTIL_SocketResult (*CheckWriteBuffer)(
            const CMUTIL_Socket *sock, long timeout);
    CMUTIL_Socket *(*ReadSocket)(
            const CMUTIL_Socket *socket,
            long timeout, CMUTIL_SocketResult *rval);
    CMUTIL_SocketResult (*WriteSocket)(
            const CMUTIL_Socket *socket,
            CMUTIL_Socket *tobesent, pid_t pid, long timeout);
    void (*GetRemoteAddr)(
            const CMUTIL_Socket *socket, char *hostbuf, int *port);
    void (*Close)(
            CMUTIL_Socket *socket);
};

CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnect(
        const char *host, int port, long timeout);

CMUTIL_API CMUTIL_Socket *CMUTIL_SSLSocketConnect(
        const char *cert, const char *key, const char *ca,
        const char *servername,
        const char *host, int port, long timeout);

typedef struct CMUTIL_ServerSocket CMUTIL_ServerSocket;
struct CMUTIL_ServerSocket {
    CMUTIL_Socket *(*Accept)(
            const CMUTIL_ServerSocket *server, long timeout);
    void (*Close)(CMUTIL_ServerSocket *server);
};

CMUTIL_API CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
        const char *host, int port, int qcnt);

CMUTIL_API CMUTIL_ServerSocket *CMUTIL_SSLServerSocketCreate(
        const char *cert, const char *key, const char *ca,
        const char *host, int port, int qcnt);


typedef enum CMUTIL_JsonType {
    CMUTIL_JsonTypeValue = 0,
    CMUTIL_JsonTypeObject,
    CMUTIL_JsonTypeArray
} CMUTIL_JsonType;

typedef struct CMUTIL_Json CMUTIL_Json;
struct CMUTIL_Json {
    void (*ToString)(
            const CMUTIL_Json *json, CMUTIL_String *buf, CMBool pretty);
    CMUTIL_JsonType (*GetType)(
            const CMUTIL_Json *json);
    CMUTIL_Json *(*Clone)(
            const CMUTIL_Json *json);
    void (*Destroy)(CMUTIL_Json *json);
};

typedef enum CMUTIL_JsonValueType {
    CMUTIL_JsonValueLong = 0,
    CMUTIL_JsonValueDouble,
    CMUTIL_JsonValueString,
    CMUTIL_JsonValueBoolean,
    CMUTIL_JsonValueNull
} CMUTIL_JsonValueType;

typedef struct CMUTIL_JsonValue CMUTIL_JsonValue;
struct CMUTIL_JsonValue {
    CMUTIL_Json	parent;
    CMUTIL_JsonValueType (*GetValueType)(
            const CMUTIL_JsonValue *jval);
    int64_t (*GetLong)(
            const CMUTIL_JsonValue *jval);
    double (*GetDouble)(
            const CMUTIL_JsonValue *jval);
    CMUTIL_String *(*GetString)(
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
    CMUTIL_String *(*GetString)(
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
    CMUTIL_String *(*GetString)(
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

#ifdef __cplusplus
}
#endif

#endif // LIBCMUTILS_H__

