# libcmutils samples

One small program per feature area. Each one is self-contained, prints what it is doing, and
returns a non-zero exit status if `CMUTIL_Clear()` reports a leak — so running a sample is also a
check that its own object handling is correct.

## Building and running

Samples are built with the library by default:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/samples/sample_01_hello
```

Turn them off with `-DBUILD_SAMPLES=OFF`.

On Linux the shared library is not in the loader path when running straight from the build tree:

```bash
LD_LIBRARY_PATH=build ./build/samples/sample_05_json
```

The data files in [`conf/`](conf) are addressed by absolute path (the build passes it in as
`SAMPLE_DATA_DIR`), so a sample can be started from any directory. Files a sample *writes* land in
the working directory.

## The samples

| Sample | Covers |
| --- | --- |
| [sample_01_hello.c](sample_01_hello.c) | `CMUTIL_Init` / `CMUTIL_Clear`, the `CMCall` convention, the allocator macros |
| [sample_02_arrays.c](sample_02_arrays.c) | `CMUTIL_Array` plain, sorted and owning; stack operations; `CMUTIL_List`; iterators |
| [sample_03_maps.c](sample_03_maps.c) | `CMUTIL_Map`, value ownership, case-insensitive keys, `GetKeys` / `GetPairs` / `PrintTo` |
| [sample_04_strings.c](sample_04_strings.c) | `CMUTIL_String`, `CMUTIL_StringArray`, the `char*` helpers, `CMUTIL_ByteBuffer`, `CMUTIL_CSConv` |
| [sample_05_json.c](sample_05_json.c) | Building, printing, parsing and walking JSON; parsing a file |
| [sample_06_xml.c](sample_06_xml.c) | Parsing XML, walking the DOM, attributes, building a document, `CMUTIL_XmlToJson` |
| [sample_07_config.c](sample_07_config.c) | `CMUTIL_Config` typed accessors, `Save` and `CMUTIL_ConfigLoad` |
| [sample_08_logging.c](sample_08_logging.c) | Level macros, appenders, named loggers, configuring in code and from `.jsonc` |
| [sample_09_threads.c](sample_09_threads.c) | `CMUTIL_Thread`, `CMUTIL_Mutex` / `CMSync`, `CMUTIL_Cond`, `CMUTIL_Semaphore`, `CMUTIL_RWLock` |
| [sample_10_threadpool.c](sample_10_threadpool.c) | `CMUTIL_ThreadPool` with a fixed and with a dynamic size |
| [sample_11_timer.c](sample_11_timer.c) | `CMUTIL_Timer` delayed, absolute and repeating tasks; `Cancel` and `Purge` |
| [sample_12_pool.c](sample_12_pool.c) | `CMUTIL_Pool` with create / destroy / validity callbacks, concurrent borrowers |
| [sample_13_files.c](sample_13_files.c) | `CMUTIL_File`, `CMUTIL_FileStream`, `CMUTIL_PathCreate`, directory listing and glob search |
| [sample_14_process.c](sample_14_process.c) | `CMUTIL_Process`: environment, streams, `Wait`, `Kill` |
| [sample_15_library.c](sample_15_library.c) | `CMUTIL_Library` loading [sample_plugin.c](sample_plugin.c) and resolving symbols |
| [sample_16_socket.c](sample_16_socket.c) | `CMUTIL_ServerSocket` / `CMUTIL_Socket` echo server and clients, addresses, TLS entry points |
| [sample_17_dgram.c](sample_17_dgram.c) | `CMUTIL_DGramSocket` connectionless and connected |
| [sample_18_http.c](sample_18_http.c) | `CMUTIL_HttpClient` GET / POST / arbitrary methods, headers, TLS verification |
| [sample_19_crypto.c](sample_19_crypto.c) | AES-CBC / CTR / GCM, RSA encryption and signatures, Base64, secure random |
| [sample_20_stackwalk.c](sample_20_stackwalk.c) | `CMUTIL_StackWalker` and the `CMLog*S` macros |

[sample_common.h](sample_common.h) holds the two helpers every sample uses: `sample_init()`, which
initializes the library and points the log system at [conf/sample_console_log.jsonc](conf/sample_console_log.jsonc),
and `sample_exit()`, which tears everything down and reports leaks.

## Samples that touch the outside world

- **sample_16_socket** binds `127.0.0.1:19999`, **sample_17_dgram** binds `127.0.0.1:19898`.
- **sample_18_http** performs real HTTPS requests against `example.com`. It warns and still exits
  successfully when the network is unavailable; pass another URL prefix as the first argument.
- **sample_14_process** spawns `/bin/sh`, `/bin/echo` (`cmd` on Windows).
- **sample_08_logging** writes `sample_app.log`, `sample_rolling.log` and a `log/` directory into
  the working directory.
- **sample_13_files** creates and removes a `sample_files/` scratch directory.

## Data files

| File | Used by |
| --- | --- |
| [conf/sample_console_log.jsonc](conf/sample_console_log.jsonc) | every sample, through `sample_init()` |
| [conf/sample_log.jsonc](conf/sample_log.jsonc) | sample_08 — console, file and rolling file appenders |
| [conf/sample_app.conf](conf/sample_app.conf) | sample_07 — properties file |
| [conf/sample_data.json](conf/sample_data.json) | sample_05 |
| [conf/sample_data.xml](conf/sample_data.xml) | sample_06 |
| [conf/sample_rsa_private.pem](conf/sample_rsa_private.pem), [conf/sample_rsa_public.pem](conf/sample_rsa_public.pem) | sample_19 — throwaway key pair, **never reuse it** |
| [cmutil_log.jsonc](cmutil_log.jsonc) | the annotated reference configuration, showing all four appender types |

## Two things that bite everyone

**Do not nest `CMCall` inside another `CMCall`'s argument list.** The trailing arguments are pasted
with `## __VA_ARGS__` and the inner call is left unexpanded, so the compiler complains about an
undeclared `CMUTIL_CALL__`. Use a temporary:

```c
uint8_t *bytes = CMCall(buf, GetBytes);
uint32_t len   = (uint32_t)CMCall(buf, GetSize);
CMCall(sock, Write, bytes, len, 1000);
```

The samples are built at C99, where that rule always holds. At C23 or C++20 the header switches to
`__VA_OPT__` and nesting is allowed — see `CMUTIL_CALL_NESTED` in the top-level README.

**Every appender in a `.jsonc` log configuration needs a `type`** — it is what selects the
implementation, and a missing one invalidates the whole file, after which the library silently falls
back to its built-in console configuration. Loggers are more forgiving: `type` is only needed to
mark the `"root"` logger, and a logger without it is a named one.
