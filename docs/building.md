# Building BiblioLED

## Current proof of concept

The C++ proof of concept uses C++11, CMake, native Win32 controls, and SQLite.

### Requirements

- CMake 3.24 or later.
- A C/C++ toolchain capable of producing a 32-bit Windows XP-compatible executable.
- For the Windows build, network access at first configure so CMake can download the pinned SQLite 3.53.4 amalgamation. CMake verifies the archive SHA3-256 hash before use.
- On non-Windows hosts, the core test target uses the system SQLite development package.

The final XP-compatible compiler/toolchain has not yet been selected. GitHub Actions also performs a Win32 compile check with a modern Windows SDK; this catches Windows-specific source errors but does not validate XP runtime compatibility. Defining `_WIN32_WINNT=0x0501` and the PE subsystem version is not, by itself, proof of XP compatibility; imported APIs and runtime dependencies must also be checked.

### Configure and build on Windows

From the repository root, using a 32-bit generator/toolchain:

```text
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The `-A Win32` option applies to Visual Studio generators. For MinGW, configure with an explicitly selected 32-bit toolchain file or compiler path instead.

The output includes `BiblioLED.exe`, `port.ico`, and `config/settings.ini`. Runtime directories are created beside the executable.

### Run core tests on a development host

On Linux or another host with CMake and SQLite development files:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The tests create their own synthetic database in the build directory. They do not use the user's database.

## XP compatibility note

SQLite 3.54.0 changed its Windows implementation to use Slim Reader/Writer Locks, which are not available on Windows XP. SQLite's maintainer states that SQLite 3.53 and earlier continue to work on XP. The prototype therefore pins 3.53.4. See the [SQLite discussion](https://sqlite.org/forum/info/2f3abc1a9d346e4a779003d2e21204255bd7462c99396ffeee46025e22fe0f48).

This is a compatibility choice based on the SQLite API dependency change, not a substitute for executing the built application on Windows XP SP3.
