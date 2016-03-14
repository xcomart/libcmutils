# libcmutils

## 1. About

libcmutils is a bunch of utility functions with object based approach.

Almost all C libraries are created with lots of long named functions,
this may causes confusion to developer who to uses it.

The libcmutils library :

* has intuitive structure to use like OOP language
* is written in pure ISO C99 code
* runs on almost every platforms
* offers log4j like logging system
* has concurrent programming API(Mutex, Thread, Semaphore, Timer, etc)
* provides simple version JSON, XML parser and builder

libcmutils is developed and maintained by Dennis Soungjin Park <xcomart@gmail.com>.

## 2. License

The source code freely to use under **GPL v3** - see [LICENSE file](LICENSE)

## 3. Features

* Implementation of commonly used data structures in object based approach.
	* Dynamic array operations
	* Dynamic string operations
	* Hashmap implementation
	* Linked List implementation
* Implementation of platform independent concurrent operations.
	* Conditional object
	* Mutex
	* Thread
	* Semaphore
	* Read/Write Lock
	* Generic Resource Pool
* Simple JSON/XML implementation.
	* JSON parser/builder with comment support
	* XML parser/builder
* Extensible logging system.
	* Log4J like implementation.
	* JSON configuration file support.
	* Custom appender can be used.
	* FileAppender, RollingFileAppender, SocketAppender and ConsoleAppender implemented internally.
	* Asynchronous logging support.
* Platform independent non-blocking TCP/IP networking implementation.
	* ServerSocket / ClientSocket implemented.
	* Passing socket to another process using IPC.
* Supports memory management by three types of strategy
	* System - Uses system version malloc, realloc, strdup and free.
	* Recycle - Creates 2^n sized memory blocks using checkout/release methods.
		This strategy checks boundary exceeding, leaking and double freeing etc.
		Proper option for production system.
	* Debug - Same as Recycle, except every memory allocation will generates
		callstack informations. Increadibly slow.
		So you may only use when memory leak detected.

## 4. Installation

### Windows platforms(Visual Studio)

under construction.

### Windows platforms(MinGW)

under construction.

### GNU (Unix / Linux) platforms

under construction.

## 6. Examples

under construction.
