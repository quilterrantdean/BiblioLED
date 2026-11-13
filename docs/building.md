# Building BiblioLED

## Current proof of concept

The C++ proof of concept uses C++11, CMake, native Win32 controls, and SQLite.

### Requirements

- CMake 3.24 or later.
- A C/C++ toolchain capable of producing a 32-bit Windows XP-compatible executable.
- For the Windows build, network access at first configure so CMake can download the pinned SQLite 3.53.4 amalgamation. CMake verifies the archive SHA3-256 hash before use.
- On non-Windows hosts, the core test target uses the system SQLite development package.

The initial XP-targeted compiler baseline is the **x86 edition of w64devkit**. Upstream documents that this edition supports Windows XP or later and requires a Pentium 4-class processor with SSE2. CI pins w64devkit 2.10.0 by SHA-256; the local workflow below uses the same CMake preset. The separate modern-MSVC job remains a source/Win32 compile check, not XP validation.

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

## XP-targeted build using w64devkit x86

Use the x86 edition of w64devkit, not the x64 edition. Its x86 toolchain targets Windows XP or later and requires SSE2; this is appropriate for the project's Pentium 4 target. The dedicated GitHub Actions workflow pins w64devkit 2.10.0 and verifies the downloaded archive against SHA-256 before extracting it.

Start the w64devkit command prompt, change to the repository root, and confirm that the compiler is the 32-bit target:

```text
gcc -dumpmachine
```

The expected target is `i686-w64-mingw32`. Then configure, build, and run the tests:

```text
cmake --preset xp-x86-w64devkit
cmake --build --preset xp-x86-w64devkit --parallel
ctest --preset xp-x86-w64devkit
```

The executable is written to `build/xp-x86-w64devkit/BiblioLED.exe`. The CI job also checks that it is a 32-bit PE executable with Windows GUI subsystem version 5.01, rejects selected post-XP imports/API-set DLLs, and publishes a disposable artifact containing the executable, icon, settings file, a short PE inspection report, and test instructions. The successful run for commit `40df8aee91227e2f08bce9175d7f52dc052c879b` passed compilation, the SQLite store test, PE inspection, and artifact upload.

These checks establish an XP-oriented build target, not proof that the application runs on XP. The ZIP is only a test prototype: it currently creates a new database automatically when the expected file is absent and does not yet implement the agreed automatic backup/recovery protection. Do not point it at or replace the real application data.

## XP compatibility note

SQLite 3.54.0 changed its Windows implementation to use Slim Reader/Writer Locks, which are not available on Windows XP. SQLite's maintainer states that SQLite 3.53 and earlier continue to work on XP. The prototype therefore pins 3.53.4. See the [SQLite discussion](https://sqlite.org/forum/info/2f3abc1a9d346e4a779003d2e21204255bd7462c99396ffeee46025e22fe0f48).

This is a compatibility choice based on the SQLite API dependency change, not a substitute for executing the built application on Windows XP SP3.


## CI validation

The `Core tests` workflow has two jobs:

- `Core tests (Ubuntu)`: builds and tests the platform-independent SQLite store.
- `Win32 compile check (modern Windows SDK)`: compiles the 32-bit Win32 application and runs the SQLite store tests on a Windows runner.

Both jobs passed for the initial proof-of-concept branch head. The Windows runner is a modern Windows environment, not Windows XP.
