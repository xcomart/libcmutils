# libcmutils

A multi-platform C99 utility library that exposes common building blocks — collections, strings, threads, sockets, JSON/XML, logging, crypto — through a consistent, object-oriented API.

[![Build](https://github.com/xcomart/libcmutils/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/xcomart/libcmutils/actions/workflows/build-and-test.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C99](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Build System: CMake](https://img.shields.io/badge/Build-CMake%20%E2%89%A5%203.10-064F8C.svg)](https://cmake.org/)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](#requirements)

---

## Why libcmutils

Plain C libraries usually expose long, flat, prefix-heavy function names: you have to remember
`mylib_string_array_insert_at_cstring()` and pass the object as the first argument every time. As a
project grows, the mapping between "what type am I holding" and "what functions apply to it" lives
only in your head.

libcmutils takes a different route. Every object is a struct whose members are function pointers, so
the methods that apply to a value are literally attached to the value. Creation is a free function,
everything else is a method:

```c
CMUTIL_String *str = CMUTIL_StringCreate();   /* constructor  */
CMCall(str, AddString, "hello");              /* method call  */
CMCall(str, Destroy);                         /* destructor   */
```

`CMCall(obj, Method, ...)` is a macro that expands to `(obj)->Method((obj), ...)`, so the redundant
receiver argument disappears from your source. The consequences are practical:

- **Discoverability** — your editor's completion list after `CMCall(str, ` is the full API surface
  for that type.
- **Uniformity** — every ownable object is released with `CMCall(obj, Destroy)`. Sockets use
  `Close`, threads use `Join`, and iterators use `Destroy`; beyond those three, the pattern never
  varies.
- **Portability** — threads, sockets, dynamic libraries, subprocesses and stack traces have one
  API across Linux, macOS and Windows. No `#ifdef _WIN32` in your code.
- **Leak accountability** — the library can manage all of its own allocations, and
  `CMUTIL_Clear()` reports at shutdown whether anything was leaked.

The trade-off is honest: each object carries a vtable-sized header, and calls go through function
pointers. In exchange you get an API that stays legible at scale.

### Two rules for CMCall

`CMCall` is a preprocessor macro, and that leaks through in exactly two places. Both are easy to
avoid once you know them, and both are silent if you do not.

**Do not nest `CMCall` inside another `CMCall`'s argument list** — unless your compiler gives you
`__VA_OPT__`. The portable spelling pastes the trailing arguments with `## __VA_ARGS__`, which is
what drops the comma for a method that takes none; the preprocessor does not macro-expand arguments
consumed by `##`, so the inner call survives as an undeclared `CMUTIL_CALL__`:

```c
/* does not compile */
CMCall(sock, Write, CMCall(buf, GetBytes), CMCall(buf, GetSize), 1000);

/* do this instead */
uint8_t *bytes = CMCall(buf, GetBytes);
uint32_t len   = (uint32_t)CMCall(buf, GetSize);
CMCall(sock, Write, bytes, len, 1000);
```

C23 and C++20 replace that paste with `__VA_OPT__`, and then the trailing arguments are expanded
normally and nesting just works. The header picks the spelling for you:

| `CMUTIL_CALL_NESTED` | when | nesting |
| --- | --- | --- |
| `1` | `__STDC_VERSION__ > 201710L`, `__cplusplus >= 202002L`, or MSVC ≥ 19.29 with `/Zc:preprocessor` | allowed |
| `0` | anything older | rejected at compile time |

Define `CMUTIL_CALL_NESTED` yourself to override it — `1` on a compiler that offers `__VA_OPT__`
outside C23, `0` to keep the portable spelling so an accidental nesting is caught. This is decided
by the compiler that *includes* the header, not the one that built the library: `CMCall` is a
preprocessor macro and emits the same call either way, so it cannot break the ABI. Building your own
code at C23 while linking a library compiled at C99 is fine.

Where it is available, the `__VA_OPT__` spelling also silences the `-pedantic` complaint that
`CMCall(obj, Destroy)` passes no variadic argument.

**The receiver may be evaluated twice.** The plain expansion of `CMCall(obj, Method)` is
`(obj)->Method((obj))`, which names `obj` twice. A variable does not care; an expression with side
effects runs twice:

```c
/* with CMUTIL_CALL_SINGLE_EVAL == 0, GetAt is called twice */
puts(CMCall(CMCall(list, GetAt, i), GetFullPath));

/* always safe */
CMUTIL_File *f = CMCall(list, GetAt, i);
puts(CMCall(f, GetFullPath));
```

Binding the receiver to a temporary needs an expression that can hold a declaration, which standard
C does not have — but a GNU statement expression and a C++ lambda both do, and the header uses
whichever is available:

| `CMUTIL_CALL_SINGLE_EVAL` | when | receiver |
| --- | --- | --- |
| `1` | GCC or Clang (C and C++), any C++ compiler with a conforming preprocessor | evaluated once |
| `0` | MSVC compiling C | evaluated twice |

Define it to `0` yourself to keep the double expansion everywhere, which is the honest setting while
your code still has to build with MSVC as C: **the single-evaluation guarantee does not exist
there**, so a side-effecting receiver stays a portability bug even where this happens to work.
Forcing it to `1` where neither extension exists is a compile error rather than a silent fallback.

## Feature overview

| Area | Types |
| --- | --- |
| Collections | `CMUTIL_Array`, `CMUTIL_List`, `CMUTIL_Map`, `CMUTIL_Iterator` |
| Text | `CMUTIL_String`, `CMUTIL_StringArray`, `CMUTIL_ByteBuffer`, `CMUTIL_CSConv` |
| Concurrency | `CMUTIL_Thread`, `CMUTIL_ThreadPool`, `CMUTIL_Mutex`, `CMUTIL_Cond`, `CMUTIL_Semaphore`, `CMUTIL_RWLock`, `CMUTIL_Timer` |
| Resource pooling | `CMUTIL_Pool` |
| Networking | `CMUTIL_Socket`, `CMUTIL_ServerSocket`, `CMUTIL_DGramSocket`, `CMUTIL_HttpClient`, `CMUTIL_RestClient` |
| Serialization | `CMUTIL_Json`, `CMUTIL_JsonObject`, `CMUTIL_JsonArray`, `CMUTIL_JsonValue`, `CMUTIL_XmlNode` |
| Cryptography | `CMUTIL_BlockCrypto`, `CMUTIL_RSACrypto`, `CMUTIL_RSAKey` |
| System | `CMUTIL_Process`, `CMUTIL_File`, `CMUTIL_FileStream`, `CMUTIL_Library`, `CMUTIL_StackWalker` |
| Configuration & logging | `CMUTIL_Config`, `CMUTIL_LogSystem`, `CMUTIL_Logger`, `CMUTIL_LogAppender` |

## Requirements

- **Language**: C99
- **Build system**: CMake >= 3.10
- **Platforms**: Linux, macOS, Windows (all three are built and tested in CI)
- **Dependencies**:
  - OpenSSL (>= 3.6.0 as pinned by the vcpkg manifest) — TLS sockets and the crypto module
  - zlib
  - libiconv (not required on Linux, where the C library provides `iconv`)

## Building and installing

### With vcpkg

`vcpkg.json` in the repository root is a manifest, so vcpkg resolves the dependencies for you when
you configure with its toolchain file:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo
```

This is the route CI uses on Windows.

### With system packages

**Linux (Debian/Ubuntu)**

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev zlib1g-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

**macOS (Homebrew)**

```bash
brew install cmake openssl zlib libiconv
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl
cmake --build build
```

If OpenSSL sits somewhere the default search paths do not cover, point `OPENSSL_PREFIX` at its
installation prefix — the build looks for `libssl`, `libcrypto` and `ssl.h` under
`${OPENSSL_PREFIX}/lib` and `${OPENSSL_PREFIX}/include` before falling back to `/usr/lib`,
`/usr/local/lib` and `/usr/local/opt/openssl`.

### Build options

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_TESTS` | `ON` | Builds the test executables and registers them with CTest |
| `CMUTIL_RSA_USE_EVP` | `ON` | Uses the OpenSSL EVP API for RSA operations instead of the legacy `RSA_*` calls |
| `OPENSSL_PREFIX` | *(empty)* | Extra prefix to search for the OpenSSL libraries and headers |

### Installing

```bash
cmake --install build --prefix /usr/local
```

Two library targets are produced and both are installed:

- `cmutils` — shared library, versioned with the contents of `VERSION`
- `cmutilsStatic` — static library (named `libcmutils.a` on non-Windows platforms)

Libraries land in `<prefix>/lib` and the single public header `libcmutils.h` in `<prefix>/include`.

## Linking it into your project

The install step does **not** export a CMake package config, so there is no `find_package(cmutils)`.
Link it the plain way:

```cmake
cmake_minimum_required(VERSION 3.10)
project(myapp LANGUAGES C)
set(CMAKE_C_STANDARD 99)

find_library(CMUTILS_LIBRARY NAMES cmutils REQUIRED)
find_path(CMUTILS_INCLUDE_DIR NAMES libcmutils.h REQUIRED)

add_executable(myapp main.c)
target_include_directories(myapp PRIVATE ${CMUTILS_INCLUDE_DIR})
target_link_libraries(myapp PRIVATE ${CMUTILS_LIBRARY})
```

Or, if you vendor the sources, `add_subdirectory()` the repository and link against the `cmutils`
(or `cmutilsStatic`) target directly.

On Windows, the header declares the API as `__declspec(dllimport)` unless `CMUTIL_EXPORT` is
defined, so consuming the DLL requires no extra flags; the DLL itself is built with `CMUTIL_EXPORT`.

## Quick start

```c
#include <stdio.h>
#include <libcmutils.h>

int main(void)
{
    /* Must be called before anything else in the library. */
    CMUTIL_Init(CMMemRecycle);

    CMUTIL_String *str = CMUTIL_StringCreate();
    CMCall(str, AddString, "hello");
    CMCall(str, AddPrint, ", %s! (v%s)", "world", CMUTIL_GetLibVersion());
    printf("%s\n", CMCall(str, GetCString));
    CMCall(str, Destroy);

    /* Releases library resources; returns CMFalse if anything leaked. */
    return CMUTIL_Clear() == CMTrue ? 0 : 1;
}
```

Every program follows the same shape: `CMUTIL_Init()` at the top, `CMUTIL_Clear()` at the bottom,
and a `Destroy` (or `Close`/`Join`) for every object you created in between.

## Module guide

### Arrays — `CMUTIL_Array`

A dynamic array of `void*` that doubles as a stack (`Push`/`Pop`/`Top`/`Bottom`) and, when you
supply a comparator, as a sorted collection with binary search. Supplying a free callback transfers
ownership of the elements to the array.

```c
/* Sorted array of strings that owns its elements. */
CMUTIL_Array *arr = CMUTIL_ArrayCreateEx(
        5, (CMCompareCB)strcmp, CMUTIL_GetMem()->Free);

CMCall(arr, Add, CMStrdup("banana"), NULL);
CMCall(arr, Add, CMStrdup("cherry"), NULL);
CMCall(arr, Add, CMStrdup("apple"), NULL);

printf("%s\n", (char*)CMCall(arr, GetAt, 0));   /* apple */

uint32_t idx = 0;
if (CMCall(arr, Find, "cherry", &idx))
    printf("cherry at %u\n", idx);

CMUTIL_Iterator *it = CMCall(arr, Iterator);
while (CMCall(it, HasNext))
    printf("- %s\n", (char*)CMCall(it, Next));
CMCall(it, Destroy);

CMCall(arr, Destroy);
```

`CMUTIL_ArrayCreate()` is shorthand for an unsorted, non-owning array with the default capacity.
Note that `Push`, `InsertAt` and `SetAt` deliberately fail on a sorted array — position is
determined by the comparator, not by the caller.

### Lists — `CMUTIL_List`

A doubly linked list with `AddFront`/`AddTail`, `RemoveFront`/`RemoveTail`, `Remove`, `GetSize`,
`Iterator` and `MoveAll` for splicing one list into another. Create it with `CMUTIL_ListCreate()`,
or `CMUTIL_ListCreateEx(freecb)` when the list should own its elements.

### Maps — `CMUTIL_Map`

A string-keyed hash map that preserves insertion order and rebuilds itself when the load factor is
exceeded. Keys are copied; values are owned only if you pass a free callback.

```c
CMUTIL_Map *map = CMUTIL_MapCreate();
CMCall(map, Put, "key1", "value1", NULL);
CMCall(map, Put, "key2", "value2", NULL);

printf("%s\n", (const char*)CMCall(map, Get, "key1"));   /* value1 */
CMCall(map, Remove, "key2");                             /* returns the value */
CMCall(map, Destroy);

/* Case-insensitive keys, map owns the values, 0.75 load factor. */
CMUTIL_Map *env = CMUTIL_MapCreateEx(
        CMUTIL_MAP_DEFAULT, CMTrue, CMFree, 0.75f);
CMCall(env, Put, "PATH", CMStrdup(getenv("PATH")), NULL);
/* CMCall(env, Get, "path") now resolves too. */
CMCall(env, Destroy);
```

`GetKeys` returns a `CMUTIL_StringArray`, `GetPairs` a `CMUTIL_Array` of `CMUTIL_MapPair`, and
`GetAt`/`RemoveAt` address entries by insertion index.

### Strings — `CMUTIL_String`, `CMUTIL_StringArray`, `CMUTIL_ByteBuffer`, `CMUTIL_CSConv`

`CMUTIL_String` is a growable text buffer with append (`AddString`, `AddNString`, `AddChar`,
`AddPrint`, `AddVPrint`, `AddAnother`), insert (`InsertString`, `InsertNString`, `InsertPrint`,
`InsertVPrint`, `InsertAnother`) and transform (`Substring`, `Replace`, `ToLower`/`SelfToLower`,
`ToUpper`/`SelfToUpper`, `SelfTrim`, `CutTailOff`, `Clone`) operations. The `Self*` variants mutate
in place; the others return a new string you own.

```c
CMUTIL_String *str = CMUTIL_StringCreate();
CMCall(str, AddString, "test");
CMCall(str, AddPrint, "-%d", 12);
CMCall(str, InsertString, "S", 2);              /* teSst-12 */
CMCall(str, SelfToUpper);                       /* TESST-12 */

CMUTIL_String *lower = CMCall(str, ToLower);    /* new object */
printf("%s / %s (%zu bytes)\n",
       CMCall(str, GetCString), CMCall(lower, GetCString),
       CMCall(str, GetSize));

CMCall(lower, Destroy);
CMCall(str, Destroy);
```

`CMUTIL_StringArray` holds `CMUTIL_String` objects and owns them. It accepts both object and C
string forms of every mutator (`Add`/`AddCString`, `InsertAt`/`InsertAtCString`,
`SetAt`/`SetAtCString`) and reads back either way (`GetAt` returns the object, `GetCString` the raw
characters).

```c
CMUTIL_StringArray *parts = CMUTIL_StringSplit("asdf:;qwer:;1234", ":;");
for (size_t i = 0; i < CMCall(parts, GetSize); i++)
    printf("%zu: %s\n", i, CMCall(parts, GetCString, i));
CMCall(parts, Destroy);
```

There are also in-place helpers for raw `char*` buffers that return a pointer into the input:
`CMUTIL_StrTrim`, `CMUTIL_StrLTrim`, `CMUTIL_StrRTrim`, `CMUTIL_StrNextToken`,
`CMUTIL_StrSkipSpaces`, plus `CMUTIL_StringHexToBytes` for hex decoding.

`CMUTIL_ByteBuffer` is the binary counterpart — `AddByte`, `AddBytes`, `AddBytesPart`,
`InsertByteAt`, `InsertBytesAt`, `GetAt`, `GetBytes`, `GetSize`, `GetCapacity`, `ShrinkTo`. It is
the currency of the socket and HTTP APIs. Each mutator returns the buffer itself, so calls chain.

`CMUTIL_CSConv` wraps iconv for character set conversion:

```c
CMUTIL_CSConv *conv = CMUTIL_CSConvCreate("EUC-KR", "UTF-8");
CMUTIL_String *utf8 = CMCall(conv, Forward, euckr_string);
CMUTIL_String *back = CMCall(conv, Backward, utf8);
CMCall(back, Destroy);
CMCall(utf8, Destroy);
CMCall(conv, Destroy);
```

### Concurrency — threads, locks and timers

One API over pthreads and Win32 threads. A `CMUTIL_Thread` is freed by `Join`, never by `Destroy` —
you must join every thread you create, even one you never started.

```c
typedef struct { int value; CMUTIL_Mutex *mutex; } Counter;

static void *worker(void *param)
{
    Counter *c = (Counter*)param;
    CMCall(c->mutex, Lock);
    c->value++;
    CMCall(c->mutex, Unlock);
    return NULL;
}

int main(void)
{
    CMUTIL_Init(CMMemRecycle);

    Counter c;
    c.mutex = CMUTIL_MutexCreate();
    c.value = 0;

    CMUTIL_Thread *t = CMUTIL_ThreadCreate(worker, &c, "worker");
    CMCall(t, Start);
    CMCall(t, Join);                  /* also frees the thread object */

    /* A pool with a dynamic size: pass a non-positive pool_size. */
    CMUTIL_ThreadPool *pool = CMUTIL_ThreadPoolCreate(-1, "Workers");
    for (int i = 0; i < 10; i++)
        CMCall(pool, Execute, (CMProcCB)worker, &c);
    CMCall(pool, Wait);               /* block until the queue drains */
    CMCall(pool, Destroy);

    printf("value = %d\n", c.value);  /* 11 */

    CMCall(c.mutex, Destroy);
    return CMUTIL_Clear() == CMTrue ? 0 : 1;
}
```

Also available:

- `CMUTIL_Mutex` — recursive, with `Lock`, `Unlock`, `TryLock`. The `CMSync(mutex, ...)` macro runs a
  block under the lock; do not `return`, `break`, `continue` or `goto` out of it, or the unlock is
  skipped.
- `CMUTIL_Cond` — manual- or auto-reset condition (event) object: `Wait`, `TimedWait`, `Set`,
  `Reset`.
- `CMUTIL_Semaphore` — counting semaphore: `Acquire` (with timeout), `Release`.
- `CMUTIL_RWLock` — reader/writer lock: `ReadLock`, `ReadUnlock`, `WriteLock`, `WriteUnlock`.
- `CMUTIL_ThreadSelf`, `CMUTIL_ThreadSelfId`, `CMUTIL_ThreadSystemSelfId` — current-thread lookup.

`CMUTIL_Timer` schedules `CMProcCB` callbacks on a small thread pool of its own:

```c
CMUTIL_Timer *timer = CMUTIL_TimerCreate();

struct timeval tv;
gettimeofday(&tv, NULL);
tv.tv_sec += 1;

/* Run once, one second from now. */
CMUTIL_TimerTask *task = CMCall(timer, ScheduleAtTime, &tv, on_tick, NULL);
CMCall(task, Cancel);

/* Or repeat every 100 ms starting now. */
gettimeofday(&tv, NULL);
task = CMCall(timer, ScheduleAtRepeat, &tv, 100, CMTrue, on_tick, NULL);
CMCall(task, Cancel);

CMCall(timer, Destroy);
```

`ScheduleDelay` and `ScheduleDelayRepeat` take a millisecond delay instead of an absolute time, and
`CMUTIL_TimerCreateEx(precision, threads)` tunes the tick resolution and worker count.

### Resource pool — `CMUTIL_Pool`

A generic pool for expensive objects (database handles, sockets, parsers). You supply create,
destroy and validity-test callbacks; the pool keeps between `initcnt` and `maxcnt` live resources,
pings idle ones on an interval, and hands them out with `CheckOut`/`Release`.

```c
CMUTIL_Pool *pool = CMUTIL_PoolCreate(
        5, 10,              /* initial / maximum resources */
        create_resource,    /* void *(*)(void *udata)                */
        destroy_resource,   /* void  (*)(void *res, void *udata)     */
        test_resource,      /* CMBool(*)(void *res, void *udata)     */
        1000,               /* ping interval in milliseconds         */
        CMFalse,            /* test on borrow                        */
        &ctx,               /* udata passed to the callbacks         */
        NULL);              /* timer; NULL creates a private one     */

void *res = CMCall(pool, CheckOut, 1000);   /* timeout in ms */
CMCall(pool, Release, res);
CMCall(pool, Destroy);
```

### Networking — `CMUTIL_Socket`, `CMUTIL_ServerSocket`

Blocking sockets with per-call timeouts. Every operation returns a `CMSocketResult`
(`CMSocketOk`, `CMSocketTimeout`, `CMSocketPollFailed`, `CMSocketReceiveFailed`,
`CMSocketSendFailed`, `CMSocketNotConnected`, `CMSocketConnectFailed`, `CMSocketBindFailed`, …), so
a timeout is a normal, distinguishable outcome rather than an error.

```c
CMUTIL_ServerSocket *srv = CMUTIL_ServerSocketCreate("0.0.0.0", 9999, 128, CMFalse);
CMUTIL_ByteBuffer *buf = CMUTIL_ByteBufferCreateEx(1024);

for (;;) {
    CMUTIL_Socket *cli = NULL;
    CMSocketResult sr = CMCall(srv, Accept, &cli, 1000);
    if (sr == CMSocketTimeout) continue;
    if (sr != CMSocketOk) break;

    CMUTIL_SocketAddr peer;
    if (CMCall(cli, GetRemoteAddr, &peer) == CMSocketOk) {
        char host[128]; int port;
        CMUTIL_SocketAddrGet(&peer, host, &port);
        printf("connected: %s:%d\n", host, port);
    }

    if (CMCall(cli, Read, buf, 4, 1000) == CMSocketOk) {
        uint8_t *bytes = CMCall(buf, GetBytes);
        uint32_t len   = (uint32_t)CMCall(buf, GetSize);
        CMCall(cli, Write, bytes, len, 1000);
    }

    CMCall(buf, Clear);
    CMCall(cli, Close);        /* sockets are closed, not destroyed */
}

CMCall(buf, Destroy);
CMCall(srv, Close);
```

The client side is `CMUTIL_SocketConnect(host, port, timeout, silent)` or
`CMUTIL_SocketConnectWithAddr(addr, timeout)`. Addresses are `CMUTIL_SocketAddr`
(a `struct sockaddr_storage`) built with `CMUTIL_SocketAddrSet(&addr, host, port)` and read back
with `CMUTIL_SocketAddrGet`.

Unix domain sockets — emulated with a loopback TCP port on Windows, where the "path" is a port
number string — are available through `CMUTIL_SocketConnectIPC` and `CMUTIL_ServerSocketCreateIPC`.
`CMUTIL_SocketPair` creates a connected pair. `CMCall(sock, SetSilent, CMTrue)` suppresses the
library's error logging on a socket, which is useful for the expected disconnects of a busy server.

A socket can also carry another socket's descriptor to a peer process over an IPC channel via
`ReadSocket`/`WriteSocket`.

### TLS and certificate verification

TLS is provided by OpenSSL (`CMUTIL_SUPPORT_SSL` and `CMUTIL_SSL_USE_OPENSSL` are defined by the
build).

```c
CMUTIL_Socket *sock = CMUTIL_SSLSocketConnect(
        NULL,               /* client certificate file (PEM), optional */
        NULL,               /* client private key file (PEM), optional */
        NULL,               /* CA file; NULL uses the system trust store */
        "example.com",      /* server name — enables verification + SNI */
        "example.com", 443, 5000);
```

> **Behaviour change — certificate verification is now on by default.**
>
> Earlier releases never called `SSL_CTX_set_verify`, so any presented certificate was accepted.
> That is no longer the case:
>
> - **CA file given** — it replaces the trust store as the sole trust anchor, `SSL_VERIFY_PEER` is
>   enabled, and the host name is additionally matched against the certificate
>   (`SSL_set1_host`).
> - **No CA file but a server name given** — the **system trust store** is loaded and
>   `SSL_VERIFY_PEER` is enabled. On Windows, OpenSSL ships no default trust store, so the OS
>   certificate store is loaded through the winstore provider
>   (`org.openssl.winstore://`) before falling back to the default verify paths.
> - **Neither given** — verification is explicitly disabled (`SSL_VERIFY_NONE`). This is now an
>   opt-out you have to ask for, not the default.
>
> Code that used to connect to a host with a self-signed or private-CA certificate will now fail
> the handshake. Supply the CA, or opt out deliberately.

`CMUTIL_SSLServerSocketCreate(cert, key, ca, host, port, qcnt)` is the server-side counterpart.

### Datagrams — `CMUTIL_DGramSocket`

UDP sockets with the same timeout-and-result-code style. Connected (`Connect`, `Send`, `Recv`) and
connectionless (`SendTo`, `RecvFrom`) use are both supported.

```c
CMUTIL_DGramSocket *sock = CMUTIL_DGramSocketCreate();
CMUTIL_SocketAddr local;
CMUTIL_SocketAddrSet(&local, "0.0.0.0", 9898);
CMCall(sock, Bind, &local);

CMUTIL_ByteBuffer *buf = CMUTIL_ByteBufferCreateEx(2048);
CMUTIL_SocketAddr remote;
if (CMCall(sock, RecvFrom, buf, &remote, 1000) == CMSocketOk) {
    CMCall(buf, InsertBytesAt, 0, (uint8_t*)"resp-", 5);
    CMCall(sock, SendTo, buf, &remote, 1000);
}

CMCall(buf, Destroy);
CMCall(sock, Close);
```

`CMUTIL_DGramSocketCreateBind(addr)` creates and binds in one step. `GetLocalAddr`, `GetRemoteAddr`,
`IsConnected` and `Disconnect` round out the type.

### HTTP client — `CMUTIL_HttpClient`

A small HTTP/HTTPS client built on the socket layer. It takes a URL prefix at construction and
relative URIs per request; bodies and responses are `CMUTIL_ByteBuffer`s.

```c
CMUTIL_Init(CMMemRecycle);

int status = 0;
CMUTIL_HttpClient *client = CMUTIL_HttpClientCreate("https://example.com");
CMCall(client, SetKeepAlive, CMFalse);

CMUTIL_ByteBuffer *body = CMCall(client, Get, NULL, "/", &status, 5000);
if (body != NULL) {
    printf("status %d, %zu bytes\n", status, CMCall(body, GetSize));
    CMCall(body, Destroy);
}

CMCall(client, Destroy);
```

`Get`, `Post` and the general `Request(method, headers, uri, body, &status, timeout)` all take an
optional `CMUTIL_Map` of request headers and a millisecond timeout, and return `NULL` on failure.
Connections are kept alive by default; `SetKeepAlive(CMFalse)` disables that.

For HTTPS, a fresh client verifies the host by default (`verify_host` on, `verify_peer` off — the
latter controls whether a *client* certificate is presented). Two knobs adjust it:

```c
/* Trust a private CA: cert, key, cafile. */
CMCall(client, SetSSLCert, NULL, NULL, "/etc/ssl/private-ca.pem");

/* Or turn verification off entirely (host, peer). */
CMCall(client, SetVerify, CMFalse, CMFalse);
```

To reach a server with a self-signed certificate you must do one or the other — the default now
rejects it.

### REST client — `CMUTIL_RestClient`

The same client speaking [`CMUTIL_Json`](#json--cmutil_json-and-friends) instead of byte buffers. It
serializes the request body, fills in `Accept` and `Content-Type: application/json` where the caller
left them out, and parses the response.

```c
CMUTIL_RestClient *rest = CMUTIL_RestClientCreate("https://api.example.com");
CMCall(rest, SetTimeout, 5000L);              /* the REST methods take no timeout */

CMUTIL_Json *user = CMCall(rest, Get, NULL, "/v1/users/42");
if (user != NULL) {
    const char *name = CMCall((CMUTIL_JsonObject*)user, GetCString, "name");
    printf("%s\n", name);
    CMUTIL_JsonDestroy(user);
}

CMUTIL_JsonObject *body = CMUTIL_JsonObjectCreate();
CMCall(body, PutString, "role", "maintainer");
CMBool ok = CMCall(rest, Put, NULL, "/v1/users/42", (CMUTIL_Json*)body);
CMUTIL_JsonDestroy(body);                     /* the body stays yours */

CMCall(&rest->base, Destroy);
```

| Method | Returns |
| --- | --- |
| `Get(headers, uri)` | the parsed response body, or `NULL` |
| `Post(headers, uri, data)` | the parsed response body, or `NULL` |
| `Put(headers, uri, data)` | `CMTrue` for any 2xx; the response is discarded |
| `Delete(headers, uri)` | nothing |
| `SetTimeout(ms)` | — starts at `CMUTIL_REST_DEFAULT_TIMEOUT` (30 s) |
| `GetStatus()` | the status of the most recent request, `0` if none arrived |

`NULL` alone does not say what went wrong, so **check `GetStatus`**: a status of `0` means the
request never reached a response — a connection failure, a timeout, a malformed reply — while any
other value means the server answered and the body was absent or not JSON. An error body *that is*
JSON comes back parsed, whatever the status, because that is where APIs put the reason:

```c
CMUTIL_Json *res = CMCall(rest, Get, NULL, "/v1/users/999");
switch (CMCall(rest, GetStatus)) {
    case 0:   /* never got there */          break;
    case 200: /* res holds the user */       break;
    default:  /* res may hold the error */   break;
}
if (res) CMUTIL_JsonDestroy(res);
```

The first member is a complete `CMUTIL_HttpClient`, so a REST client *is* one: TLS settings,
keep-alive, non-JSON requests and `Destroy` all go through `&rest->base`. Note that a REST client
has no `Destroy` of its own — destroying the base destroys both halves. The status is per client,
so one client performs one request at a time.

### JSON — `CMUTIL_Json` and friends

`CMUTIL_Json` is the common base; `CMUTIL_JsonObject`, `CMUTIL_JsonArray` and `CMUTIL_JsonValue` are
the concrete types. Cast to `CMUTIL_Json*` for `GetType`, `ToString` and `Clone`, and use
`CMUTIL_JsonDestroy(x)` to release any of them.

```c
CMUTIL_JsonObject *obj = CMUTIL_JsonObjectCreate();
CMCall(obj, PutString,  "name",   "libcmutils");
CMCall(obj, PutLong,    "answer", 42);
CMCall(obj, PutDouble,  "ratio",  0.1234);
CMCall(obj, PutBoolean, "ok",     CMTrue);

CMUTIL_String *buf = CMUTIL_StringCreate();
CMCall((CMUTIL_Json*)obj, ToString, buf, CMTrue);   /* CMTrue = pretty print */
printf("%s\n", CMCall(buf, GetCString));

/* Parsing goes the other way. */
CMCall(buf, Clear);
CMCall(buf, AddString, "{\"arr\":[1,2,3],\"obj\":{\"k\":\"v\"}}");
CMUTIL_Json *parsed = CMUTIL_JsonParse(buf);
if (parsed && CMCall(parsed, GetType) == CMJsonTypeObject) {
    CMUTIL_JsonObject *po = (CMUTIL_JsonObject*)parsed;
    CMUTIL_JsonArray *arr = (CMUTIL_JsonArray*)CMCall(po, Get, "arr");
    printf("first = %" PRId64 "\n", CMCall(arr, GetLong, 0));
}

if (parsed) CMUTIL_JsonDestroy(parsed);
CMCall(buf, Destroy);
CMUTIL_JsonDestroy(obj);
```

Both containers offer typed accessors (`GetLong`, `GetDouble`, `GetString`, `GetCString`,
`GetBoolean`) and typed mutators (`PutLong`/`AddLong`, `PutNull`/`AddNull`, …) so you rarely need to
touch `CMUTIL_JsonValue` directly. The parser accepts comments, which is what makes the `.jsonc`
logging configuration below possible.

### XML — `CMUTIL_XmlNode`

A small DOM: parse from a `CMUTIL_String` (`CMUTIL_XmlParse`), a C string
(`CMUTIL_XmlParseString`) or a file (`CMUTIL_XmlParseFile`); build by hand with
`CMUTIL_XmlNodeCreate(CMXmlNodeTag, "name")` and `AddChild`; navigate with `ChildCount`, `ChildAt`,
`GetParent`, `GetName`, `GetType`, `GetAttribute`, `GetAttributeNames`; serialize with
`ToDocument(pretty)`. `CMUTIL_XmlToJson(node)` converts a document to the JSON model.

```c
CMUTIL_String *src = CMUTIL_StringCreateEx(0,
        "<xml><a attr=\"v\">1</a><b>2</b></xml>");
CMUTIL_XmlNode *node = CMUTIL_XmlParse(src);

CMUTIL_String *doc = CMCall(node, ToDocument, CMFalse);
printf("%s\n", CMCall(doc, GetCString));

CMUTIL_Json *json = CMUTIL_XmlToJson(node);
CMUTIL_JsonDestroy(json);

CMCall(doc, Destroy);
CMCall(src, Destroy);
CMCall(node, Destroy);
```

### Cryptography — `CMUTIL_BlockCrypto`, `CMUTIL_RSACrypto`

Block ciphers are created from algorithm, mode, padding and key size, then reused across
operations; the key and IV are passed per call.

```c
uint8_t key[32] = { /* 256-bit key */ };
uint8_t iv[16]  = { /* 128-bit IV  */ };

CMUTIL_BlockCrypto *aes =
        CMUTIL_BlockCryptoCreate("AES", "CBC", "PKCS5Padding", 256);

CMUTIL_String *plain     = CMUTIL_StringCreateEx(0, "hello block crypto");
CMUTIL_String *encrypted = CMCall(aes, Encrypt, plain, key, iv);
CMUTIL_String *decrypted = CMCall(aes, Decrypt, encrypted, key, iv);

CMCall(decrypted, Destroy);
CMCall(encrypted, Destroy);
CMCall(plain, Destroy);
CMCall(aes, Destroy);
```

Supported algorithms are `AES` (CBC, GCM, ECB, CFB/CFB128, OFB, CTR), `DES` (CBC, ECB, CFB, OFB),
`DESede`/`TripleDES` (CBC, ECB, CFB, OFB) and `SEED` (CBC, ECB, CFB, OFB). Passing `"NoPadding"`
disables block padding; any other padding name (for example `"PKCS5Padding"`) enables it.

> **GCM tag convention.** GCM is an AEAD mode, and this library uses a self-contained framing for
> it: `Encrypt` appends the **16-byte authentication tag directly after the ciphertext**, and
> `Decrypt` expects it there, strips it and verifies it. Do not transport the tag separately.
> Decryption of a GCM payload shorter than 16 bytes is rejected, and block padding is always
> disabled in GCM regardless of the `padding` argument.

RSA keys are loaded from PEM text or PEM files, and the crypto object handles encryption,
decryption, signing and verification:

```c
CMUTIL_PrivateKey *priv = CMUTIL_PrivateKeyCreateFromPEM(priv_pem, (const uint8_t*)"");
CMUTIL_PublicKey  *pub  = CMUTIL_PublicKeyCreateFromPEM(pub_pem);
CMUTIL_RSACrypto  *rsa  = CMUTIL_RSACryptoCreate();

CMUTIL_String *plain     = CMUTIL_StringCreateEx(0, "hello rsa");
CMUTIL_String *encrypted = CMCall(rsa, EncryptWithPublicKey, plain, pub);
CMUTIL_String *decrypted = CMCall(rsa, DecryptWithPrivateKey, encrypted, priv);

CMUTIL_String *sig = CMCall(rsa, Sign, plain, priv);
CMBool valid = CMCall(rsa, VerifySignature, plain, sig, pub);

CMCall(sig, Destroy);
CMCall(decrypted, Destroy);
CMCall(encrypted, Destroy);
CMCall(plain, Destroy);
CMCall(rsa, Destroy);
CMCall(pub, Destroy);
CMCall(priv, Destroy);
```

`CMUTIL_PrivateKeyCreateFromFile` / `CMUTIL_PublicKeyCreateFromFile` read the same formats from
disk, and `CMCall(key, GetEncoded)` returns the DER encoding. Public-key encryption and
private-key decryption use OAEP padding; the private-encrypt/public-decrypt pair uses PKCS#1 v1.5,
which OAEP does not support in that direction. `Sign`/`VerifySignature` use SHA-256.

Two standalone helpers round out the module: `CMUTIL_CryptoRandom(buf, len)` for
cryptographically strong random bytes, and `CMUTIL_CryptoToBase64(data, len)` /
`CMUTIL_CryptoFromBase64(str)` for Base64.

### Subprocesses — `CMUTIL_Process`

Spawn a child process with an explicit working directory, environment map and argument list, then
optionally attach to its standard streams.

```c
CMUTIL_Map *env = CMUTIL_MapCreateEx(CMUTIL_MAP_DEFAULT, CMFalse, CMFree, 0.75f);
CMCall(env, Put, "PATH", CMStrdup(getenv("PATH")), NULL);

CMUTIL_Process *proc = CMUTIL_ProcessCreate(".", env, "openssl", "help", "sha256");
if (CMCall(proc, Start, CMProcStreamNone)) {
    int exit_code = CMCall(proc, Wait, 10000);   /* timeout in ms */
    printf("exited with %d\n", exit_code);
}

CMCall(proc, Destroy);
CMCall(env, Destroy);
```

`CMUTIL_ProcessCreate` is a macro that NULL-terminates the argument list for you; the underlying
`CMUTIL_ProcessCreateEx` requires the trailing `NULL` explicitly. The stream mode is a combination
of `CMProcStreamNone`, `CMProcStreamRead`, `CMProcStreamWrite`, `CMProcStreamReadWrite` and
`CMProcStreamReadErr`, and enables the matching `Read`, `Write` and `ReadErr` methods. `PipeTo`
connects one process's stdout to another's stdin; `Suspend`, `Resume`, `Kill`, `GetPid`,
`GetCommand`, `GetWorkDir`, `GetArgs` and `GetEnv` complete the interface.

### Files, directories and dynamic libraries

`CMUTIL_File` covers path inspection (`IsFile`, `IsDirectory`, `IsExists`, `Length`, `GetName`,
`GetFullPath`, `ModifiedTime`), whole-file access (`GetContents`, `Delete`), directory listing
(`Children`, returning a `CMUTIL_FileList`), glob search (`Find`) and streaming
(`CreateStream(CMFileOpenRead | CMFileOpenWrite | CMFileOpenAppend)` returning a
`CMUTIL_FileStream` with `Read`, `Write`, `Close`). `CMUTIL_PathCreate(path, mode)` creates a
directory tree.

```c
CMUTIL_File *dir = CMUTIL_FileCreate(".");
CMUTIL_FileList *found = CMCall(dir, Find, "*.c", CMTrue);
for (size_t i = 0; i < CMCall(found, Count); i++) {
    CMUTIL_File *f = CMCall(found, GetAt, i);
    printf("%s\n", CMCall(f, GetFullPath));
}
CMCall(found, Destroy);
CMCall(dir, Destroy);
```

`CMUTIL_Library` loads a shared object or DLL — the file extension may be omitted — and resolves
symbols by name:

```c
CMUTIL_Library *lib = CMUTIL_LibraryCreate("mylib");
void *fn = CMCall(lib, GetProcedure, "my_function");
CMCall(lib, Destroy);
```

### Glob patterns

The `Find` method above is backed by an internal glob engine (a port of David R. Tribble's
*fpattern*, `src/pattern.c`). It supports `?` (any single character except a path separator), `*`
(zero or more non-separator characters), sets and ranges `[abc]`, `[a-z]`, negated ranges `[!a-z]`,
`!` to negate a whole path segment, `/` and `\` as interchangeable path separators, and `\` (Unix)
or `` ` `` (DOS) as the quote character. Matching is case-insensitive. The engine is used through
`CMUTIL_File`; it is not part of the public header.

### Configuration files — `CMUTIL_Config`

A typed key/value store backed by a plain properties file.

```c
CMUTIL_Config *conf = CMUTIL_ConfigCreate();
CMCall(conf, Set,        "server.host", "0.0.0.0");
CMCall(conf, SetLong,    "server.port", 9999L);
CMCall(conf, SetBoolean, "server.tls",  CMTrue);
CMCall(conf, SetDouble,  "server.load", 0.75);
CMCall(conf, Save, "server.conf");
CMCall(conf, Destroy);

conf = CMUTIL_ConfigLoad("server.conf");
const char *host = CMCall(conf, Get,     "server.host");
long        port = CMCall(conf, GetLong, "server.port");
CMCall(conf, Destroy);
```

### Call stacks — `CMUTIL_StackWalker`

Captures the current call stack, symbolizing frames where the platform allows it.

```c
CMUTIL_StackWalker *walker = CMUTIL_StackWalkerCreate();

CMUTIL_StringArray *frames = CMCall(walker, GetStack, 0);   /* skip depth */
for (size_t i = 0; i < CMCall(frames, GetSize); i++)
    printf("%s\n", CMCall(frames, GetCString, i));
CMCall(frames, Destroy);

CMUTIL_String *out = CMUTIL_StringCreate();
CMCall(walker, PrintStack, out, 0);
CMCall(out, Destroy);

CMCall(walker, Destroy);
```

The same machinery powers the `CMLog*S` logging macros and the `CMMemDebug` allocator.

## Memory management

`CMUTIL_Init()` chooses, once per process, how the library allocates:

| Strategy | Behaviour | Use for |
| --- | --- | --- |
| `CMMemSystem` | Direct `malloc`/`calloc`/`realloc`/`strdup`/`free`, no bookkeeping | Running under valgrind, DUMA or another external memory debugger |
| `CMMemRecycle` | Allocations rounded up to power-of-two blocks and recycled; boundary corruption detected on free; leaks reported at shutdown | General use, including production |
| `CMMemDebug` | Same as recycle, plus a captured call stack for every allocation | Hunting a specific leak. Very slow |

Whichever you pick, use the library's allocators for memory that library objects will own, so that
`CMMemRecycle` and `CMMemDebug` can account for it:

```c
CMAlloc(size)            /* malloc  */
CMCalloc(nmemb, size)    /* calloc  */
CMRealloc(ptr, size)     /* realloc */
CMStrdup(str)            /* strdup  */
CMFree(ptr)              /* free    */
```

These are macros over `CMUTIL_GetMem()`, which returns the active `CMUTIL_Mem` operator table — the
same table you pass as a free callback when a collection should own its elements
(`CMUTIL_MapCreateEx(..., CMFree, ...)`).

`CMUTIL_Clear()` tears everything down and returns `CMFalse` if any allocation is still outstanding
under `CMMemRecycle` or `CMMemDebug` (it always returns `CMTrue` under `CMMemSystem`, which tracks
nothing). Returning it as your process exit status turns every test run into a leak check — which is
exactly what this project's own tests do:

```c
return CMUTIL_Clear() == CMTrue ? 0 : 1;
```

After `CMUTIL_Clear()` the library can be initialized again.

## Logging

The logging system is configured as a set of *appenders* (where output goes) attached to *loggers*
(named, hierarchical, level-filtered sources). Declare a logger name once per source file and use
the level macros:

```c
#include <libcmutils.h>

CMUTIL_LogDefine("myapp.module")

void do_work(void)
{
    CMLogInfo("starting, pid=%d", getpid());
    CMLogDebug("value = %d", 42);
    CMLogErrorS("failed");        /* the trailing S appends a stack trace */
}
```

Available macros: `CMLogTrace`, `CMLogDebug`, `CMLogInfo`, `CMLogWarn`, `CMLogError`, `CMLogFatal`,
each with a stack-trace variant (`CMLogTraceS`, …), plus `CMLog(level, fmt, ...)` /
`CMLogS(level, ...)` taking a `CMLogLevel` value and `CMLogIsEnabled(level)` for guarding expensive
message construction. If no log system has been configured, messages fall back to a built-in
minimal writer, so logging never crashes an unconfigured program.

### Configuring from JSON

`CMUTIL_LogSystemGet()` lazily configures itself the first time it is called: it reads the file
named by the `CMUTIL_LOG_CONFIG` environment variable, or `cmutil_log.jsonc` in the working
directory. Because the parser accepts comments, the configuration file can be annotated. A complete
example lives in [`samples/cmutil_log.jsonc`](samples/cmutil_log.jsonc):

```json
{
    "configuration": {
        "appenders": [
            {
                "name": "SampleConsoleAppender",
                "pattern": "%d %P-[%10t] [%-15F:%04L] [%-5p] %c - %m%ex%n",
                "async": true,
                "useStderr": true,
                "asyncBufferSize": 512,
                "type": "Console"
            },
            {
                "name": "SampleRollingFileAppender",
                "pattern": "%d %P-[%10t] [%-15F:%04L] [%-5p] %c - %m%ex%n",
                "async": true,
                "asyncBufferSize": 512,
                "type": "RollingFile",
                "fileName": "log/sample_rolling_file_log.log",
                "rollTerm": "day"
            }
        ],
        "loggers": [
            {
                "type": "root",
                "level": "INFO",
                "appenderRef": ["SampleConsoleAppender", "SampleRollingFileAppender"]
            },
            {
                "name": "cmutil.network",
                "additivity": false,
                "level": "TRACE",
                "appenderRef": "SampleConsoleAppender"
            }
        ]
    }
}
```

Appender `type` is one of `Console`, `File`, `RollingFile` or `Socket`; `rollTerm` is one of `year`,
`month`, `day`, `hour`, `minute`. A logger with `"type": "root"` is the fallback for every logger
name; named loggers match by dotted prefix and `additivity: false` stops a message from also
reaching the parent's appenders.

The **closest matching logger decides the level**. `cmutil.network` above is configured at `TRACE`,
so every logger under that name emits from `TRACE` up even though the root logger stops at `INFO` —
and a named logger set to `WARN` under a `DEBUG` root suppresses everything below `WARN` for its
names, including at the root's appenders. `CMLogIsEnabled()` reports that same effective level.

> Every appender needs a `type`, since it selects the implementation, and a missing one invalidates
> the whole file — the library then silently falls back to its built-in console configuration. For a
> logger the key is optional: it only has to be spelled out to mark the `root` logger, and anything
> else (or nothing at all) means a named logger.

Pattern conversions (each accepts a printf-style width, e.g. `%-15F`, `%04L`):

| Token | Meaning |
| --- | --- |
| `%d` / `%date` | Timestamp. Braces take either a named format — `DEFAULT`, `ISO8601`, `ISO8601_BASIC`, `ABSOLUTE`, `COMPACT`, `GENERAL` — as in `%d{ISO8601}`, or a `strftime` string containing a percent sign, where `%Q` and `%q` expand to milliseconds |
| `%c` / `%logger` | Logger name |
| `%t` / `%tid` / `%thread` | Thread id or name |
| `%P` / `%pid` / `%process` | Process id |
| `%F` / `%file` | Source file name |
| `%L` / `%line` | Source line number |
| `%p` / `%level` | Log level; supports remapping, e.g. `%p{TRACE=_,DEBUG=D,INFO=' '}` |
| `%m` / `%msg` / `%message` | The message |
| `%e{NAME}` / `%env{NAME}` | Value of an environment variable |
| `%ex` / `%s` / `%stack` | Stack trace, when logged with a `*S` macro |
| `%n` | Line separator |

### Configuring in code

The same structure can be built programmatically, which is what the tests do:

```c
CMUTIL_LogSystem *lsys = CMUTIL_LogSystemCreate();

/* NULL name creates the root logger. */
CMUTIL_ConfLogger *logger =
        CMCall(lsys, CreateLogger, NULL, CMLogLevel_Debug, CMTrue);

const char *pattern = "%d{DEFAULT} %P-[%-10t] (%-15F:%04L) [%05p] %c : %m%n%ex";

CMUTIL_LogAppender *console =
        CMUTIL_LogConsoleAppenderCreate("Console", pattern, CMFalse);
CMCall(console, SetAsync, 64);              /* buffered, non-blocking */
CMCall(logger, AddAppender, console, CMLogLevel_Debug);

CMUTIL_LogAppender *file =
        CMUTIL_LogFileAppenderCreate("File", "app.log", pattern);
CMCall(logger, AddAppender, file, CMLogLevel_Info);

CMUTIL_LogSystemSet(lsys);   /* the log system takes ownership */
```

The other constructors are `CMUTIL_LogRollingFileAppenderCreate(name, fpath, logterm, rollpath,
pattern)` and `CMUTIL_LogSocketAppenderCreate(name, accept_host, listen_port, pattern)` — the latter
listens for connections and streams the log to whoever attaches (`telnet localhost <port>`).
`CMCall(lsys, UpdateEnv)` re-establishes the logging threads in a child after `fork()`.

`CMUTIL_LogSystemConfigureFomJson(path)` builds the same thing from a file. It installs the result
as the global log system itself, destroying the one it replaces, so call it and ignore the return
value — passing that value to `CMUTIL_LogSystemSet` does nothing, because setting the system that is
already installed is a no-op. A missing or invalid file is not a failure either: a built-in console
configuration is installed instead.

## Running the tests

Tests are built by default (`BUILD_TESTS=ON`) and registered with CTest.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

cd build
ctest --output-on-failure                 # everything
ctest -C RelWithDebInfo --extra-verbose   # multi-config generators (MSVC)
ctest -R string_test --output-on-failure  # one test by name
ctest -N                                  # list without running
```

The registered tests are `array_test`, `concurrent_test`, `config_test`, `crypto_test`,
`dgram_test`, `http_test`, `json_test`, `log_test`, `map_test`, `network_test`, `pool_test`,
`process_test`, `string_test`, `timer_test` and `xml_test`.

Every test initializes with `CMMemRecycle` and returns a failure status if `CMUTIL_Clear()` reports
a leak, so a green run is also a clean-memory run. Note that some tests reach outside the process:
`network_test` and `dgram_test` bind local ports, `http_test` performs a real HTTPS request, and
`process_test` and `crypto_test` expect an `openssl` executable on `PATH`.

CI (`.github/workflows/build-and-test.yml`) runs this on `ubuntu-latest`, `macos-latest` and
`windows-latest` in `RelWithDebInfo` for every push and pull request against `main`/`master`.

## Running the samples

`samples/` holds one annotated program per feature area — twenty-one of them, from `sample_01_hello`
through `sample_21_rest`. They are built with the library by default (`BUILD_SAMPLES=ON`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

LD_LIBRARY_PATH=build ./build/samples/sample_05_json
```

Like the tests, every sample returns a failure status when `CMUTIL_Clear()` reports a leak. See
[`samples/README.md`](samples/README.md) for the full list, the data files each one reads, and which
of them bind ports or reach the network.

## Project structure

```
src/                Library sources and the single public header
  libcmutils.h        Public API — everything is declared here
  arrays.c            CMUTIL_Array
  lists.c             CMUTIL_List
  maps.c              CMUTIL_Map
  strings.c           CMUTIL_String, StringArray, ByteBuffer, CSConv
  concurrent.c        Threads, mutexes, conditions, semaphores, RW locks, timers
  pool.c              CMUTIL_Pool
  network.c           TCP sockets, server sockets, TLS
  datagram.c          UDP sockets
  http.c              CMUTIL_HttpClient, CMUTIL_RestClient
  nanojson.c          JSON parser and model
  nanoxml.c           XML parser and model
  crypto.c            Block ciphers, RSA, Base64, secure random
  process.c           CMUTIL_Process
  logger.c            Log system, loggers, appenders
  config.c            CMUTIL_Config
  callstack.c         CMUTIL_StackWalker
  pattern.c           Internal glob matcher (fpattern)
  memdebug.c          Recycling and debugging allocators
  base.c, system.c    Initialization, files, dynamic libraries, platform glue
  functions.h         Internal declarations shared between sources
  platforms.h         Platform detection and compatibility shims
test/               One executable per module, all registered with CTest
samples/            One annotated program per feature area
  sample_NN_*.c       The samples themselves, in reading order
  sample_common.h     Shared init/teardown helpers
  sample_plugin.c     Tiny shared library, loaded by sample_15_library
  conf/               Configurations, documents and keys the samples read
  cmutil_log.jsonc    Annotated reference logging configuration
CMakeLists.txt      Build definition
vcpkg.json          Dependency manifest
VERSION             Version string, read at configure time
```

## Contributing

Issues and pull requests are welcome at
<https://github.com/xcomart/libcmutils>. Before opening a pull request:

1. Build with `BUILD_TESTS=ON` and make sure `ctest` passes on your platform — a leak reported by
   `CMUTIL_Clear()` fails the run, so watch for that too.
2. Add or extend a test under `test/` for behaviour you change, and register it in
   `test/CMakeLists.txt` with `add_test_target(<name>)` if it is a new file.
3. Follow the surrounding style: object types are structs of function pointers, constructors are
   `CMUTIL_XxxCreate[Ex]`, and every allocation goes through the `CMAlloc`/`CMFree` family.
4. Keep the public surface in `src/libcmutils.h`, documented with the same doxygen comment style as
   its neighbours. Internal helpers belong in `src/functions.h`.

Participation is governed by the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

MIT — see [LICENSE](LICENSE).

The bundled glob matcher in `src/pattern.c` derives from *fpattern* by David R. Tribble.

## Authors

**Dennis Soungjin Park** — <xcomart@gmail.com>
