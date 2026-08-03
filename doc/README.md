# API reference

**Published at <https://xcomart.github.io/libcmutils/api/>**, rebuilt by the
`Docs` workflow on every push that touches the header, this directory, the
README or the Jekyll config.

The reference is generated from the doc comments in
[`src/libcmutils.h`](../src/libcmutils.h), which is the whole public API — the
`.c` files are implementation and are deliberately left out.

## Building it

```bash
cd doc && doxygen
```

or through the build, which also fills in the version number:

```bash
cmake -S . -B build
cmake --build build --target docs
```

The `docs` target appears only when doxygen is installed, and it is never part
of `all` — the reference is built on request. Output lands in `doc/html`; open
`doc/html/index.html`. Neither the output nor `doxygen.log` is committed.

Graphviz is not needed. Doxygen 1.9 or newer is expected; older versions still
work but ignore some of the HTML settings.

## What it contains

The front page covers the two things to know before reading anything else: that
an object is a struct of function pointers reached through `CMCall`, and the
`CMUTIL_Init` / `CMUTIL_Clear` lifecycle.

**Topics** is the way in. The API is grouped by subject rather than listed
alphabetically, and within a group the declaration order of the header is kept,
so a type is followed by its methods and then its constructor:

| Topic | Covers |
| --- | --- |
| Fixed width integers, CMBool and the platform shims | `CMBool`, the integer limits, the MSVC and macOS shims |
| The CMCall convention | `CMCall`, `CMUTIL_CALL_NESTED`, `CMUTIL_CALL_SINGLE_EVAL` |
| Initialization and memory operations | `CMUTIL_Init`, `CMUTIL_Clear`, `CMUTIL_Mem`, the allocator macros |
| Threads, locks and synchronization primitives | `CMUTIL_Thread`, `CMUTIL_ThreadPool`, `CMUTIL_Mutex`, `CMUTIL_Cond`, `CMUTIL_Semaphore`, `CMUTIL_RWLock` |
| Arrays, maps, lists and their iterator | `CMUTIL_Array`, `CMUTIL_Map`, `CMUTIL_List`, `CMUTIL_Iterator` |
| Strings, string arrays, byte buffers and charset conversion | `CMUTIL_String`, `CMUTIL_StringArray`, `CMUTIL_ByteBuffer`, `CMUTIL_CSConv`, the `CMUTIL_Str*` helpers |
| XML parsing and the document model | `CMUTIL_XmlNode` and the parsers |
| JSON parsing and the document model | `CMUTIL_Json`, `CMUTIL_JsonObject`, `CMUTIL_JsonArray`, `CMUTIL_JsonValue` |
| Scheduled and repeating tasks | `CMUTIL_Timer`, `CMUTIL_TimerTask` |
| Generic resource pool | `CMUTIL_Pool` |
| Dynamic library loading | `CMUTIL_Library` |
| Files, directories and file streams | `CMUTIL_File`, `CMUTIL_FileList`, `CMUTIL_FileStream` |
| Configuration files | `CMUTIL_Config` |
| Log system, loggers and appenders | `CMUTIL_LogSystem`, the appenders, the `CMLog*` macros |
| Call stack capture | `CMUTIL_StackWalker` |
| Sockets, datagrams and the HTTP and REST clients | `CMUTIL_Socket`, `CMUTIL_ServerSocket`, `CMUTIL_DGramSocket`, `CMUTIL_HttpClient`, `CMUTIL_RestClient` |
| Child process creation and control | `CMUTIL_Process` |
| Block ciphers, RSA, Base64 and secure random | `CMUTIL_BlockCrypto`, `CMUTIL_RSACrypto`, the key types |

For prose rather than a reference, read the [project README](../README.md); for
working code, [`samples/`](../samples) has one annotated program per subject.

## Keeping it honest

The configuration leaves `EXTRACT_ALL` off and turns every documentation
warning on, so an undocumented entity is reported rather than published as a
blank page:

```
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_INCOMPLETE_DOC = YES
WARN_NO_PARAMDOC       = YES
```

Warnings go to `doc/doxygen.log`. **It should be empty.** If a run leaves
anything in it, that is a doc comment to fix — a missing `@param`, a `@param`
naming an argument that no longer exists, a missing `@return` — not a message
to ignore.

One thing worth knowing when editing the header: `@typedef` and `@struct` are
Doxygen commands that take a *declaration*, not a name and a description. A
comment sitting directly above the entity already documents it, so a plain
`@brief` is what belongs there; writing `@typedef CMUTIL_Foo Some description`
creates a phantom symbol and leaves the real one undocumented.
