# libcmutils

`libcmutils` is a collection of C utility functions implemented with an object-oriented approach. It aims to simplify C development by providing intuitive, well-structured APIs for common tasks, avoiding the confusion often caused by traditional C libraries with long, inconsistent function names.

## Features

- **Object-Oriented Data Structures**: Intuitive wrappers for common data structures.
  - Dynamic Arrays (`CMUTIL_Array`)
  - Dynamic Strings (`CMUTIL_String`, `CMUTIL_StringArray`)
  - Hashmaps (`CMUTIL_Map`)
  - Linked Lists (`CMUTIL_List`)
  - Byte Buffers (`CMUTIL_ByteBuffer`)
- **Concurrent Programming**: Platform-independent API for multi-threaded applications.
  - Threads, Mutexes, Semaphores, Read/Write Locks
  - Condition Variables, Resource Pools, Timers
- **Networking**: High-level, platform-independent TCP/IP and UDP networking.
  - Blocking and Non-blocking Socket operations
  - SSL/TLS support (OpenSSL backend)
  - ServerSocket and ClientSocket implementations
  - Datagram (UDP) support
- **Serialization**: JSON and XML support.
  - JSON parser/builder with comment support
  - XML parser/builder
  - XML to JSON conversion
- **Logging System**: Flexible, Log4j-like logging.
  - Support for multiple appenders: Console, File, Rolling File, and Socket
  - Asynchronous logging support
  - JSON-based configuration
- **Memory Management**: Three pluggable strategies.
  - **System**: Standard `malloc`/`free`.
  - **Recycle**: High-performance strategy with boundary checking and leak detection (recommended for production).
  - **Debug**: Slow strategy with full callstack information for tracking down leaks.

## Stack & Requirements

- **Language**: C99
- **Build System**: CMake (>= 3.10)
- **Package Manager**: vcpkg (recommended)
- **Dependencies**:
  - `libiconv`
  - `openssl` (optional, for SSL support)

## Installation & Setup

### Using vcpkg (Recommended)

1. Install dependencies:
   ```bash
   vcpkg install libiconv openssl
   ```

2. Configure and build:
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   ```

### Manual Build

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

## Running Tests

Tests are built by default if `BUILD_TESTS` option is ON.

```bash
# Build a specific test (e.g., string_test)
cmake --build build --target string_test

# Run the test
./build/test/string_test
```

Available test targets: `array_test`, `concurrent_test`, `config_test`, `dgram_test`, `json_test`, `log_test`, `network_test`, `string_test`.

## Project Structure

- `src/`: Core library source code and `libcmutils.h`.
- `test/`: Unit tests for various modules.
- `samples/`: Example usage of the library (TODO: add more samples).
- `CMakeLists.txt`: Build configuration.
- `vcpkg.json`: Dependency manifest.

## Environment Variables

- TODO: Document any runtime environment variables used by the library.

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## Authors

- **Dennis Soungjin Park** - [xcomart@gmail.com](mailto:xcomart@gmail.com)
