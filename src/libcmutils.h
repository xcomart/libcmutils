/*
					  GNU General Public License
	libcmutil is a bunch of commonly used utility functions for multiplatform.
	Copyright (C) 2016 Dennis Soungjin Park

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
 *
 */
#ifndef __LIBCMUTILS_H__
#define __LIBCMUTILS_H__

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

#if defined(_MSC_VER)
# define strcasecmp	_stricmp
# define stat		_stat
# define pid_t		DWORD
#endif

#define CMUTIL_APPEND(a,b)	a ## b

#if defined(_MSC_VER)
# define i64(x)		CMUTIL_APPEND(x, i64)
# define ui64(x)	CMUTIL_APPEND(x, ui64)
#else
# define i64(x)		CMUTIL_APPEND(x, LL)
# define ui64(x)	CMUTIL_APPEND(x, ULL)
#endif

#if !defined(int64)
# if defined(_MSC_VER)
typedef __int64 int64;
# else
typedef long long int64;
# endif
# define int64 int64
#endif
#if !defined(uint64)
# if defined(_MSC_VER)
typedef unsigned __int64 uint64;
# else
typedef unsigned long long uint64;
# endif
# define uint64 uint64
# if !defined(INT64_MAX)
#  define INT64_MAX		i64(0x7FFFFFFFFFFFFFFF)
# endif
# if !defined(UINT64_MAX)
#  define UINT64_MAX	ui64(0xFFFFFFFFFFFFFFFF)
# endif
#endif
#if !defined(PRINT64I)
# if defined(_MSC_VER)
#  define PRINT64I	"%I64d"
#  define PRINT64U	"%I64u"
# else
#  define PRINT64I	"%lld"
#  define PRINT64U	"%llu"
# endif
#endif

#if defined(APPLE)
// redefine gethostbyname for consistancy of gethostbyname_r implementation.
# define gethostbyname      CMUTIL_NetworkGetHostByName
# define gethostbyname_r    CMUTIL_NetworkGetHostByNameR
#endif

#define CMUTIL_UNUSED(a,...)	CMUTIL_UnusedP((void*)a, ## __VA_ARGS__)

#define __CMUTIL_CALL(a,b,...)	(a)->b((a), ## __VA_ARGS__)
#define CMUTIL_CALL	__CMUTIL_CALL

typedef enum CMUTIL_MemOper {
	CMUTIL_MemSystem = 0,
	CMUTIL_MemDebug,
	CMUTIL_MemRecycle
} CMUTIL_MemOper;

/// \brief Initialize this library. This function must be called before calling
/// any function in this library.
///
///  <ul>
///	  <li>CMUTIL_MemSystem: All memory operation will be replaced with
///			malloc, realloc, strdup and free.
///			same as direct call system calls.</li>
///   <li>CMUTIL_MemDebug: Option for memory debugging.
///			Any memory operation will be traced including stack tracing.
///			Recommended only for memory debugging.
///			Much slower than any other options.</li>
///	  <li>CMUTIL_MemRecycle: All memory allocation will be managed with pool.
///			Pool is consist of memory blocks which size is power of 2 bytes.
///			Recommended for any purpose. This option prevents heap memory
///			fragments and checks memory leak at termination. But a little
///			overhead required for recycling and needs much more memory
///			usage.</li>
///  </ul>
///
/// \param memoper	Memory operation initializer. Must be one of
///  CMUTIL_MemRelease, CMUTIL_MemDebug and CMUTIL_MemRecycle.
///
CMUTIL_API void CMUTIL_Init(CMUTIL_MemOper memoper);

/// \brief Clear allocated resource for this library.
CMUTIL_API void CMUTIL_Clear();

CMUTIL_API void CMUTIL_UnusedP(void*,...);

typedef struct CMUTIL_Mem_st {
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
} CMUTIL_Mem_st;

/// \brief Global memory operator structure.
///  This object will be initialized appropriate memory operators in
///  <tt>CMUTIL_Init</tt>.
extern CMUTIL_Mem_st *__CMUTIL_Mem;

/// \brief Allocate new memory. Use this macro instead of malloc
///  for debugging memory operations.
#define CMAlloc		__CMUTIL_Mem->Alloc

/// \brief Allocate new memory with zero filled.
///  Use this macro instead of calloc.
#define CMCalloc	__CMUTIL_Mem->Calloc

/// \brief Reallocate memory with given size preserving previous data.
#define CMRealloc	__CMUTIL_Mem->Realloc

/// \brief String duplication.
#define CMStrdup	__CMUTIL_Mem->Strdup

/// \brief Deallocate memory which allocated with
///  CMAlloc, CMCalloc, CMRealloc and CMStrdup.
#define CMFree		__CMUTIL_Mem->Free

/// \brief Boolean definition for this library.
///
/// Boolean definition for this library.
typedef enum CMUTIL_Bool {
	CMUTIL_True			=0x0001,
	CMUTIL_False		=0x0000
} CMUTIL_Bool;


/// \brief Platform independent condition definition for concurrency control.
///
/// Condition(or Event)
typedef struct CMUTIL_Cond CMUTIL_Cond;
struct CMUTIL_Cond {

	/// \brief Wait for this condition is set.
	///
	/// This operation is blocked until this condition is set.
	/// \param cond this condition object.
	void (*Wait)(
			CMUTIL_Cond *cond);

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
	CMUTIL_Bool (*TimedWait)(
			CMUTIL_Cond *cond,
			long millisec);

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
	void (*Set)(
			CMUTIL_Cond *cond);

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
	void (*Reset)(
			CMUTIL_Cond *cond);

	/// \brief Destroys all resources related with this object.
	/// \param cond this condition object.
	void (*Destroy)(
			CMUTIL_Cond *cond);
};

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
CMUTIL_API CMUTIL_Cond *CMUTIL_CondCreate(CMUTIL_Bool manual_reset);


/// \brief Platform independent mutex implementation.
typedef struct CMUTIL_Mutex CMUTIL_Mutex;
struct CMUTIL_Mutex {

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
	void (*Lock)(CMUTIL_Mutex *mutex);

	/// \brief Unlock this mutex object.
	///
	/// This mutex object shall be unlocked if lock count reaches zero.
	/// \param mutex This mutex object.
	void (*Unlock)(CMUTIL_Mutex *mutex);

	/// \brief Try to lock given mutex object.
	/// \param	mutex	a mutex object to be tested.
	/// \return	CMUTIL_True if mutex locked successfully,
	///			CMUTIL_False if lock failed.
	CMUTIL_Bool (*TryLock)(CMUTIL_Mutex *mutex);

	/// \brief Destroys all resources related with this object.
	/// \param mutex This mutex object.
	void (*Destroy)(CMUTIL_Mutex *mutex);
};

/// \brief Creates a mutex object.
///
/// Any thread of the calling process can use created mutex-object in a call
/// to lock methods. When a lock method returns, the waiting thread is
/// released to continue its execution.
/// This function creates a 'recursive lockable' mutex object.
CMUTIL_API CMUTIL_Mutex *CMUTIL_MutexCreate();



/// \brief Platform independent thread object.
typedef struct CMUTIL_Thread CMUTIL_Thread;
struct CMUTIL_Thread {

	/// \brief Starts this thread.
	///
	/// This function creates a new thread which is not detached,
	/// so the created thread must be joined by calling Join method.
	/// \param thread This thread object.
	/// \return This thread have been started successfully or not.
	CMUTIL_Bool (*Start)(CMUTIL_Thread *thread);

	/// \brief Join this thread.
	///
	/// This function joins this thread and clean up it's resources including
	/// this thread object, so this references are must not be used after
	/// calling this method. Every threads which been created by this library
	/// must be joined by calling this method.
	/// \param thread This thread object.
	void *(*Join)(CMUTIL_Thread *thread);

	/// \brief This thread is running or not.
	///
	/// \param thread This thread object.
	/// \return CMUTIL_True if this thread is running,
	///			CMUTIL_False if this thread is not running.
	CMUTIL_Bool (*IsRunning)(CMUTIL_Thread *thread);

	/// \brief Get the ID fo this thread. Returned ID is not system thread ID.
	///		just internal thread index.
	/// \return ID of this thread.
	unsigned int(*GetId)(CMUTIL_Thread *thread);

	/// \brief Get the name of this thread.
	/// \return Name of this thread.
	const char *(*GetName)(CMUTIL_Thread *thread);
};

/// \brief Creates a thread object.
///
/// Created thread does not start automatically.
/// Call <tt>Start</tt> method to start thread.
/// Returned object must be free by calling <tt>Join</tt> method,
/// even if thread not started.
///
/// \param proc Start routine of the created thread.
/// \param udata This argument is passed as the sole argument of 'proc'
/// \param name Thread name, any name could be assigned,
///		 and can also duplicable. But must not be exceed 200 bytes.
/// \return Created thread object.
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadCreate(
		void*(*proc)(void*), void *udata, const char *name);

/// \brief Get the ID of current thread. This id is not system thread id,
///		just internal thread index.
/// \return ID of current thread.
CMUTIL_API unsigned int CMUTIL_ThreadSelfId();

/// \brief Get current thread context.
/// \return Current thread context.
CMUTIL_API CMUTIL_Thread *CMUTIL_ThreadSelf();

/// \brief Get system dependent thread id.
/// \return System dependent thread id.
CMUTIL_API uint64 CMUTIL_ThreadSystemSelfId();

/// \brief Platform independent semaphore object.
typedef struct CMUTIL_Semaphore CMUTIL_Semaphore;
struct CMUTIL_Semaphore {

	/// \brief Acquire an ownership from semaphore.
	///
	/// Acquire an ownership in given time or fail with timed out. This method
	/// will blocked until an ownership acquired successfully in time,
	/// or the waiting time expired.
	///
	/// \param semaphore This semaphore object.
	/// \param millisec Waiting time to acquire an ownership in millisecond.
	/// \return CMUTIL_True if an ownership acquired successfully in time,
	///			CMUTIL_False if failed to acquire ownership in time.
	CMUTIL_Bool (*Acquire)(CMUTIL_Semaphore *semaphore, long millisec);

	/// \brief Release an ownership to semaphore.
	///
	/// Release an ownership to semaphore, the semaphore value increased
	/// greater than zero consequently, one of the thread which blocked in
	/// Acquire method will get the ownership and be unblocked.
	/// \param semaphore This semaphore object.
	void (*Release)(CMUTIL_Semaphore *semaphore);

	/// \brief Destroy all resources related with this object.
	/// \param semaphore This semaphore object.
	void (*Destroy)(CMUTIL_Semaphore *semaphore);
};

/// \brief Creates a semaphore object.
///
/// Create an in-process semaphore object.
/// \param initcnt Initial semaphore ownership count.
/// \return Created semaphore object.
CMUTIL_API CMUTIL_Semaphore *CMUTIL_SemaphoreCreate(int initcnt);



/// \brief Platform independent read/write lock object.
typedef struct CMUTIL_RWLock CMUTIL_RWLock;
struct CMUTIL_RWLock {

    /// \brief Read lock for this object.
    ///
    /// This method shall apply a read lock to given object. The calling thread
    /// acquires the read lock if a writer does not hold the lock and
    /// there are no writers blocked on the lock.
    /// \param rwlock This read-write lock object.
	void (*ReadLock)(CMUTIL_RWLock *rwlock);

    /// \brief Read unlock for this object.
    ///
    /// This method shall release a read lock held on given object.
    /// \param rwlock This read-write lock object.
    void (*ReadUnlock)(CMUTIL_RWLock *rwlock);

    /// \brief Write lock for this object.
    ///
    /// This method whall apply a write lock to given object. The calling thread
    /// acquires the write lock if no other thread (reader or writer) holds
    /// given object. Otherwise, the thread shall block until it can acquire
    /// the lock. The calling thread may deadlock if at the time
    /// the call is made it holds the read-write lock
    /// (whether a read or write lock).
    /// \param rwlock This read-write lock object.
    void (*WriteLock)(CMUTIL_RWLock *rwlock);

    /// \brief Write unlock for this object.
    ///
    /// This method shall release a write lock held on given object.
    /// \param rwlock This read-write lock object.
    void (*WriteUnlock)(CMUTIL_RWLock *rwlock);

    /// \brief Destroy all resources related with this object.
    /// \param rwlock This read-write lock object.
    void (*Destroy)(CMUTIL_RWLock *rwlock);
};

/// \brief Create a read-write lock object.
CMUTIL_API CMUTIL_RWLock *CMUTIL_RWLockCreate();


typedef struct CMUTIL_Iterator CMUTIL_Iterator;
struct CMUTIL_Iterator {
	CMUTIL_Bool (*HasNext)(CMUTIL_Iterator *iter);
	void *(*Next)(CMUTIL_Iterator *iter);
	void (*Destroy)(CMUTIL_Iterator *iter);
};


typedef struct CMUTIL_Array CMUTIL_Array;
struct CMUTIL_Array {
	void *(*Add)(CMUTIL_Array *array, void *item);
	void *(*Remove)(CMUTIL_Array *array, const void *compval);
	void *(*InsertAt)(CMUTIL_Array *array, void *item, int index);
	void *(*RemoveAt)(CMUTIL_Array *array, int index);
	void *(*SetAt)(CMUTIL_Array *array, void *item, int index);
	void *(*GetAt)(CMUTIL_Array *array, int index);
	void *(*Find)(CMUTIL_Array *array, const void *compval, int *index);
	int  (*GetSize)(CMUTIL_Array *array);
	void (*Push)(CMUTIL_Array *array, void *item);
	void *(*Pop)(CMUTIL_Array *array);
	void *(*Top)(CMUTIL_Array *array);
	void *(*Bottom)(CMUTIL_Array *array);
	CMUTIL_Iterator *(*Iterator)(CMUTIL_Array *array);
	void (*Clear)(CMUTIL_Array *array);
	void (*Destroy)(CMUTIL_Array *array);
};

#define CMUTIL_ARRAY_DEFAULT	10
#define CMUTIL_ArrayCreate()	\
		CMUTIL_ArrayCreateEx(CMUTIL_ARRAY_DEFAULT, NULL, NULL)
CMUTIL_API CMUTIL_Array *CMUTIL_ArrayCreateEx(
		int initcapacity,
		int (*comparator)(const void*,const void*),
		void(*freecb)(void*));


typedef struct CMUTIL_String CMUTIL_String;
struct CMUTIL_String {
	int (*AddString)(
			CMUTIL_String *string, const char *tobeadded);
	int (*AddNString)(
			CMUTIL_String *string, const char *tobeadded, int size);
	int (*AddChar)(
			CMUTIL_String *string, char tobeadded);
	int (*AddPrint)(
			CMUTIL_String *string, const char *fmt, ...);
	int (*AddVPrint)(
			CMUTIL_String *string, const char *fmt, va_list args);
	int (*AddAnother)(
			CMUTIL_String *string, CMUTIL_String *tobeadded);
	int (*InsertString)(
			CMUTIL_String *string, const char *tobeadded, int at);
	int (*InsertNString)(
			CMUTIL_String *string, const char *tobeadded, int at, int size);
	int (*InsertPrint)(
			CMUTIL_String *string, int idx, const char *fmt, ...);
	int (*InsertVPrint)(
			CMUTIL_String *string, int idx, const char *fmt, va_list args);
	int (*InsertAnother)(
			CMUTIL_String *string, int idx, CMUTIL_String *tobeadded);
	void (*CutTailOff)(
			CMUTIL_String *string, int length);
	CMUTIL_String *(*Substring)(
			CMUTIL_String *string, int offset, int length);
	CMUTIL_String *(*ToLower)(
			CMUTIL_String *string);
	void (*SelfToLower)(
			CMUTIL_String *string);
	CMUTIL_String *(*ToUpper)(
			CMUTIL_String *string);
	void (*SelfToUpper)(
			CMUTIL_String *string);
	CMUTIL_String *(*Replace)(
			CMUTIL_String *string, const char *needle, const char *alter);
	int (*GetSize)(
			CMUTIL_String *string);
	const char *(*GetCString)(
			CMUTIL_String *string);
	void (*Clear)(
			CMUTIL_String *string);
	CMUTIL_String *(*Clone)(
			CMUTIL_String *string);
	void (*Destroy)(
			CMUTIL_String *string);
};

#define CMUTIL_STRING_DEFAULT	32
#define CMUTIL_StringCreate()	\
		CMUTIL_StringCreateEx(CMUTIL_STRING_DEFAULT, NULL)
CMUTIL_API CMUTIL_String *CMUTIL_StringCreateEx(
		int initcapacity,
		const char *initcontent);


typedef struct CMUTIL_StringArray CMUTIL_StringArray;
struct CMUTIL_StringArray {
	void (*Add)(
			CMUTIL_StringArray *array, CMUTIL_String *string);
	void (*AddCString)(
			CMUTIL_StringArray *array, const char *string);
	void (*InsertAt)(
			CMUTIL_StringArray *array, CMUTIL_String *string, int index);
	void (*InsertAtCString)(
			CMUTIL_StringArray *array, const char *string, int index);
	CMUTIL_String *(*RemoveAt)(
			CMUTIL_StringArray *array, int index);
	CMUTIL_String *(*SetAt)(
			CMUTIL_StringArray *array, CMUTIL_String *string, int index);
	CMUTIL_String *(*SetAtCString)(
			CMUTIL_StringArray *array, const char *string, int index);
	CMUTIL_String *(*GetAt)(
			CMUTIL_StringArray *array, int index);
	const char *(*GetCString)(
			CMUTIL_StringArray *array, int index);
	int (*GetSize)(
			CMUTIL_StringArray *array);
	CMUTIL_Iterator *(*Iterator)(
			CMUTIL_StringArray *array);
	void (*Destroy)(
			CMUTIL_StringArray *array);
};

#define CMUTIL_STRINGARRAY_DEFAULT	32
#define CMUTIL_StringArrayCreate()	\
		CMUTIL_StringArrayCreateEx(CMUTIL_STRINGARRAY_DEFAULT)
CMUTIL_API CMUTIL_StringArray *CMUTIL_StringArrayCreateEx(int initcapacity);

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
	void (*PutAll)(CMUTIL_Map *map, CMUTIL_Map *src);
	void *(*Get)(CMUTIL_Map *map, const char* key);
	void *(*Remove)(CMUTIL_Map *map, const char* key);
	CMUTIL_StringArray *(*GetKeys)(CMUTIL_Map *map);
	int (*GetSize)(CMUTIL_Map *map);
	CMUTIL_Iterator *(*Iterator)(CMUTIL_Map *map);
	void (*Clear)(CMUTIL_Map *map);
	void (*ClearLink)(CMUTIL_Map *map);
	void (*Destroy)(CMUTIL_Map *map);
	void (*PrintTo)(CMUTIL_Map *map, CMUTIL_String *out);
};

#define CMUTIL_MAP_DEFAULT	256
#define CMUTIL_MapCreate()	CMUTIL_MapCreateEx(CMUTIL_MAP_DEFAULT, NULL)
CMUTIL_API CMUTIL_Map *CMUTIL_MapCreateEx(int bucketsize, void(*freecb)(void*));


typedef struct CMUTIL_List CMUTIL_List;
struct CMUTIL_List {
	void (*AddFront)(CMUTIL_List *list, void *data);
	void (*AddTail)(CMUTIL_List *list, void *data);
	void *(*GetFront)(CMUTIL_List *list);
	void *(*GetTail)(CMUTIL_List *list);
	void *(*RemoveFront)(CMUTIL_List *list);
	void *(*RemoveTail)(CMUTIL_List *list);
	void *(*Remove)(CMUTIL_List *list, void *data);
	int (*GetSize)(CMUTIL_List *list);
	CMUTIL_Iterator *(*Iterator)(CMUTIL_List *list);
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
	CMUTIL_StringArray *(*GetAttributeNames)(CMUTIL_XmlNode *node);
	CMUTIL_String *(*GetAttribute)(
			CMUTIL_XmlNode *node, const char *key);
	void (*SetAttribute)(
			CMUTIL_XmlNode *node, const char *key, const char *value);
	int (*ChildCount)(CMUTIL_XmlNode *node);
	void (*AddChild)(CMUTIL_XmlNode *node, CMUTIL_XmlNode *child);
	CMUTIL_XmlNode *(*ChildAt)(CMUTIL_XmlNode *node, int index);
	CMUTIL_XmlNode *(*GetParent)(CMUTIL_XmlNode *node);
	const char *(*GetName)(CMUTIL_XmlNode *node);
	void (*SetName)(CMUTIL_XmlNode *node, const char *name);
	CMUTIL_XmlNodeKind (*GetType)(CMUTIL_XmlNode *node);
	CMUTIL_String *(*ToDocument)(CMUTIL_XmlNode *node, CMUTIL_Bool beutify);
	void (*SetUserData)(CMUTIL_XmlNode *node, void *udata, void(*freef)(void*));
	void *(*GetUserData)(CMUTIL_XmlNode *node);
	void (*Destroy)(CMUTIL_XmlNode *node);
};

CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParse(CMUTIL_String *str);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseString(
		const char *xmlstr, int len);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlParseFile(const char *fpath);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreate(
		CMUTIL_XmlNodeKind type, const char *tagname);
CMUTIL_API CMUTIL_XmlNode *CMUTIL_XmlNodeCreateWithLen(
		CMUTIL_XmlNodeKind type, const char *tagname, int len);




typedef struct CMUTIL_CSConv CMUTIL_CSConv;
struct CMUTIL_CSConv {
	CMUTIL_String *(*Forward)(
			CMUTIL_CSConv *conv, CMUTIL_String *instr);
	CMUTIL_String *(*Backward)(
			CMUTIL_CSConv *conv, CMUTIL_String *instr);
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

#define CMUTIL_TIMER_PRECISION	100
#define CMUTIL_TIMER_THREAD		10
#define CMUTIL_TimerCreate()	\
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
		CMUTIL_Bool (*testproc)(void *resource, void *udata),
		long pinginterval,
		CMUTIL_Bool testonborrow,
		void *udata,
		CMUTIL_Timer *timer);




typedef struct CMUTIL_Library CMUTIL_Library;
struct CMUTIL_Library {
	void *(*GetProcedure)(
			CMUTIL_Library *lib,
			const char *proc_name);
	void (*Destroy)(
			CMUTIL_Library *lib);
};

CMUTIL_API CMUTIL_Library *CMUTIL_LibraryCreate(const char *path);


typedef struct CMUTIL_FileList CMUTIL_FileList;
typedef struct CMUTIL_File CMUTIL_File;
struct CMUTIL_FileList {
	int (*Count)(
			CMUTIL_FileList *flist);
	CMUTIL_File *(*GetAt)(
			CMUTIL_FileList *flist,
			int index);
	void (*Destroy)(
			CMUTIL_FileList *flist);
};

struct CMUTIL_File {
    CMUTIL_String *(*GetContents)(
            CMUTIL_File *file);
    CMUTIL_Bool (*Delete)(
            CMUTIL_File *file);
    CMUTIL_Bool (*IsFile)(
            CMUTIL_File *file);
    CMUTIL_Bool (*IsDirectory)(
            CMUTIL_File *file);
    CMUTIL_Bool (*IsExists)(
            CMUTIL_File *file);
    long (*Length)(
            CMUTIL_File *file);
    const char *(*GetName)(
            CMUTIL_File *file);
    const char *(*GetFullPath)(
            CMUTIL_File *file);
    time_t (*ModifiedTime)(
            CMUTIL_File *file);
    CMUTIL_FileList *(*Children)(
            CMUTIL_File *file);
    CMUTIL_FileList *(*Find)(
            CMUTIL_File *file, const char *pattern, CMUTIL_Bool recursive);
    void (*Destroy)(
            CMUTIL_File *file);
};

CMUTIL_API CMUTIL_File *CMUTIL_FileCreate(const char *path);

CMUTIL_Bool CMUTIL_PathCreate(const char *path, int mode);

typedef struct CMUTIL_Config CMUTIL_Config;
struct CMUTIL_Config {
	void (*Save)(CMUTIL_Config *conf, const char *path);
	const char *(*Get)(CMUTIL_Config *conf, const char *key);
	void (*Set)(CMUTIL_Config *conf, const char *key, const char *value);
	long (*GetLong)(CMUTIL_Config *conf, const char *key);
	void (*SetLong)(CMUTIL_Config *conf, const char *key, long value);
	double (*GetDouble)(CMUTIL_Config *conf, const char *key);
	void (*SetDouble)(CMUTIL_Config *conf, const char *key, double value);
	void(*Destroy)(CMUTIL_Config *conf);
};

CMUTIL_API CMUTIL_Config *CMUTIL_ConfigCreate();
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
		CMUTIL_ConfLogger *logger,
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
		CMUTIL_Bool printStack,
		const char *fmt,
		...);
};

struct CMUTIL_LogAppender {
	const char *(*GetName)(CMUTIL_LogAppender *appender);
	void(*SetAsync)(CMUTIL_LogAppender *appender, int buffersz);
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

#define CMUTIL_LogDefine(name)											\
	static CMUTIL_Logger *___logger = NULL;								\
	static CMUTIL_Logger *__CMUTIL_GetLogger() {						\
		if (___logger == NULL) {										\
			CMUTIL_LogSystem *lsys = CMUTIL_LogSystemGet();				\
			if (lsys) ___logger =										\
				CMUTIL_CALL(CMUTIL_LogSystemGet(), GetLogger, name);	\
		}																\
		return ___logger;												\
	}

#define __CMUTIL_Log(level,stack,f,...)	do {							\
	CMUTIL_Logger *logger = __CMUTIL_GetLogger();						\
	if (logger)															\
		logger->LogEx(logger, CMUTIL_LogLevel_##level,__FILE__,__LINE__,\
					  CMUTIL_##stack,f,##__VA_ARGS__);					\
	} while(0)
#define __CMUTIL_Log2(level,stack,f,...)	do {						\
	CMUTIL_Logger *logger = __CMUTIL_GetLogger();						\
	if (logger)															\
		logger->LogEx(logger, level,__FILE__,__LINE__,					\
					  CMUTIL_##stack,f,##__VA_ARGS__);					\
	} while(0)
#define __CMUTIL_Log3(logger,level,stack,f,...)	do {					\
	if (logger)															\
		logger->LogEx(logger, level,__FILE__,__LINE__,					\
					  CMUTIL_##stack,f,##__VA_ARGS__);					\
	} while(0)

#define CMLogTrace(f,...)	__CMUTIL_Log(Trace,False,f,##__VA_ARGS__)
#define CMLogTraceS(f,...)	__CMUTIL_Log(Trace,True ,f,##__VA_ARGS__)

#define CMLogDebug(f,...)	__CMUTIL_Log(Debug,False,f,##__VA_ARGS__)
#define CMLogDebugS(f,...)	__CMUTIL_Log(Debug,True ,f,##__VA_ARGS__)

#define CMLogInfo(f,...)	__CMUTIL_Log(Info ,False,f,##__VA_ARGS__)
#define CMLogInfoS(f,...)	__CMUTIL_Log(Info ,True ,f,##__VA_ARGS__)

#define CMLogWarn(f,...)	__CMUTIL_Log(Warn ,False,f,##__VA_ARGS__)
#define CMLogWarnS(f,...)	__CMUTIL_Log(Warn ,True ,f,##__VA_ARGS__)

#define CMLogError(f,...)	__CMUTIL_Log(Error,False,f,##__VA_ARGS__)
#define CMLogErrorS(f,...)	__CMUTIL_Log(Error,True ,f,##__VA_ARGS__)

#define CMLogFatal(f,...)	__CMUTIL_Log(Fatal,False,f,##__VA_ARGS__)
#define CMLogFatalS(f,...)	__CMUTIL_Log(Fatal,True ,f,##__VA_ARGS__)

#define CMLog(l,f,...)		__CMUTIL_Log2(l,False,f,##__VA_ARGS__)
#define CMLogS(l,f,...)		__CMUTIL_Log2(l,True ,f,##__VA_ARGS__)

#define CMLogger			__CMUTIL_GetLogger()

#define CMLog2(logger,lvl,f,...)	\
	__CMUTIL_Log3(logger,lvl,f,##__VA_ARGS__)

typedef struct CMUTIL_LogSystem CMUTIL_LogSystem;
struct CMUTIL_LogSystem {
	void(*AddAppender)(CMUTIL_LogSystem *logsys, CMUTIL_LogAppender *appender);
	CMUTIL_ConfLogger *(*CreateLogger)(
		CMUTIL_LogSystem *logsys,
		const char *name,
		CMUTIL_LogLevel level,
		CMUTIL_Bool additivity);
	CMUTIL_Logger *(*GetLogger)(CMUTIL_LogSystem *logsys, const char *name);
	void(*Destroy)(CMUTIL_LogSystem *logsys);
};

CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemCreate();
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemConfigureFomJson(
		const char *jsonfile);
CMUTIL_API CMUTIL_LogSystem *CMUTIL_LogSystemGet();


typedef struct CMUTIL_StackWalker CMUTIL_StackWalker;
struct CMUTIL_StackWalker {
	CMUTIL_StringArray *(*GetStack)(CMUTIL_StackWalker *walker, int skipdepth);
	void(*PrintStack)(
		CMUTIL_StackWalker *walker, CMUTIL_String *outbuf, int skipdepth);
	void(*Destroy)(CMUTIL_StackWalker *walker);
};

CMUTIL_API CMUTIL_StackWalker *CMUTIL_StackWalkerCreate();

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
			CMUTIL_Socket *socket,
			CMUTIL_String *buffer, int size, long timeout);
	CMUTIL_SocketResult (*Write)(
            CMUTIL_Socket *socket, CMUTIL_String *data, long timeout);
	CMUTIL_SocketResult (*WritePart)(
            CMUTIL_Socket *socket, CMUTIL_String *data,
            int offset, int length, long timeout);
	CMUTIL_SocketResult (*CheckReadBuffer)(CMUTIL_Socket *sock, long timeout);
	CMUTIL_SocketResult (*CheckWriteBuffer)(CMUTIL_Socket *sock, long timeout);
	CMUTIL_Socket *(*ReadSocket)(
			CMUTIL_Socket *socket, long timeout, CMUTIL_SocketResult *rval);
	CMUTIL_SocketResult (*WriteSocket)(
			CMUTIL_Socket *socket,
			CMUTIL_Socket *tobesent, pid_t pid, long timeout);
	void (*GetRemoteAddr)(
			CMUTIL_Socket *socket, char *hostbuf, int *port);
	void (*Close)(
			CMUTIL_Socket *socket);
};

CMUTIL_API CMUTIL_Socket *CMUTIL_SocketConnect(
		const char *host, int port, long timeout);

typedef struct CMUTIL_ServerSocket CMUTIL_ServerSocket;
struct CMUTIL_ServerSocket {
	CMUTIL_Socket *(*Accept)(CMUTIL_ServerSocket *server, long timeout);
	void (*Close)(CMUTIL_ServerSocket *server);
};

CMUTIL_API CMUTIL_ServerSocket *CMUTIL_ServerSocketCreate(
		const char *host, int port, int qcnt);


typedef enum CMUTIL_JsonType {
	CMUTIL_JsonTypeValue = 0,
	CMUTIL_JsonTypeObject,
	CMUTIL_JsonTypeArray
} CMUTIL_JsonType;

typedef struct CMUTIL_Json CMUTIL_Json;
struct CMUTIL_Json {
	void (*ToString)(CMUTIL_Json *json, CMUTIL_String *buf, CMUTIL_Bool pretty);
	CMUTIL_JsonType (*GetType)(CMUTIL_Json *json);
	CMUTIL_Json *(*Clone)(CMUTIL_Json *json);
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
			CMUTIL_JsonValue *jval);
	int64 (*GetLong)(
			CMUTIL_JsonValue *jval);
	double (*GetDouble)(
			CMUTIL_JsonValue *jval);
	CMUTIL_String *(*GetString)(
			CMUTIL_JsonValue *jval);
	const char *(*GetCString)(
			CMUTIL_JsonValue *jval);
	CMUTIL_Bool (*GetBoolean)(
			CMUTIL_JsonValue *jval);
	void (*SetLong)(
			CMUTIL_JsonValue *jval, int64 inval);
	void (*SetDouble)(
			CMUTIL_JsonValue *jval, double inval);
	void (*SetString)(
			CMUTIL_JsonValue *jval, const char *inval);
	void (*SetBoolean)(
			CMUTIL_JsonValue *jval, CMUTIL_Bool inval);
	void (*SetNull)(
			CMUTIL_JsonValue *jval);
};

CMUTIL_API CMUTIL_JsonValue *CMUTIL_JsonValueCreate();

typedef struct CMUTIL_JsonObject CMUTIL_JsonObject;
struct CMUTIL_JsonObject {
	CMUTIL_Json parent;
	CMUTIL_StringArray *(*GetKeys)(
			CMUTIL_JsonObject *jobj);
	CMUTIL_Json *(*Get)(
			CMUTIL_JsonObject *jobj, const char *key);
	int64 (*GetLong)(
			CMUTIL_JsonObject *jobj, const char *key);
	double (*GetDouble)(
			CMUTIL_JsonObject *jobj, const char *key);
	CMUTIL_String *(*GetString)(
			CMUTIL_JsonObject *jobj, const char *key);
	const char *(*GetCString)(
			CMUTIL_JsonObject *jobj, const char *key);
	CMUTIL_Bool (*GetBoolean)(
			CMUTIL_JsonObject *jobj, const char *key);
	void (*Put)(
			CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Json *json);
	void (*PutLong)(
			CMUTIL_JsonObject *jobj, const char *key, int64 value);
	void (*PutDouble)(
			CMUTIL_JsonObject *jobj, const char *key, double value);
	void (*PutString)(
			CMUTIL_JsonObject *jobj, const char *key, const char *value);
	void (*PutBoolean)(
			CMUTIL_JsonObject *jobj, const char *key, CMUTIL_Bool value);
	void (*PutNull)(
			CMUTIL_JsonObject *jobj, const char *key);
	CMUTIL_Json *(*Remove)(
			CMUTIL_JsonObject *jobj, const char *key);
};

CMUTIL_API CMUTIL_JsonObject *CMUTIL_JsonObjectCreate();

typedef struct CMUTIL_JsonArray CMUTIL_JsonArray;
struct CMUTIL_JsonArray {
	CMUTIL_Json parent;
	int (*GetSize)(
			CMUTIL_JsonArray *jarr);
	CMUTIL_Json *(*Get)(
			CMUTIL_JsonArray *jarr, int index);
	int64 (*GetLong)(
			CMUTIL_JsonArray *jarr, int index);
	double (*GetDouble)(
			CMUTIL_JsonArray *jarr, int index);
	CMUTIL_String *(*GetString)(
			CMUTIL_JsonArray *jarr, int index);
	const char *(*GetCString)(
			CMUTIL_JsonArray *jarr, int index);
	CMUTIL_Bool (*GetBoolean)(
			CMUTIL_JsonArray *jarr, int index);
	void (*Add)(
			CMUTIL_JsonArray *jarr, CMUTIL_Json *json);
	void (*AddLong)(
			CMUTIL_JsonArray *jarr, int64 value);
	void (*AddDouble)(
			CMUTIL_JsonArray *jarr, double value);
	void (*AddString)(
			CMUTIL_JsonArray *jarr, const char *value);
	void (*AddBoolean)(
			CMUTIL_JsonArray *jarr, CMUTIL_Bool value);
	void (*AddNull)(
			CMUTIL_JsonArray *jarr);
	CMUTIL_Json *(*Remove)(
			CMUTIL_JsonArray *jarr, int index);
};

CMUTIL_API CMUTIL_JsonArray *CMUTIL_JsonArrayCreate();
CMUTIL_API CMUTIL_Json *CMUTIL_JsonParse(CMUTIL_String *jsonstr);
#define CMUTIL_JsonDestroy(a)	CMUTIL_CALL((CMUTIL_Json*)(a), Destroy)

CMUTIL_API CMUTIL_Json *CMUTIL_XmlToJson(CMUTIL_XmlNode *node);

#ifdef __cplusplus
}
#endif

#endif // __LIBCMUTILS_H__

