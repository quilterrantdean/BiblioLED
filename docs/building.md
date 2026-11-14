# Building BiblioLED

## Current proof of concept

The C++ proof of concept uses C++11, CMake, native Win32 controls, and SQLite.

### Requirements

- CMake 3.24 or later.
- A C/C++ toolchain capable of producing a 32-bit Windows XP-compatible executable.
- For the Windows build, network access at first configure so CMake can download the pinned SQLite 3.53.4 amalgamation and zlib 1.4 source archive. CMake verifies both archives against their pinned hashes before use; zlib is built statically.
- On non-Windows hosts, the core test target uses the system SQLite and zlib development packages (for Debian/Ubuntu: `libsqlite3-dev` and `zlib1g-dev`).

The initial XP-targeted compiler baseline is the **x86 edition of w64devkit**. Upstream documents that this edition supports Windows XP or later and requires a Pentium 4-class processor with SSE2. CI pins w64devkit 2.10.0 by SHA-256; the local workflow below uses the same CMake preset. The separate modern-MSVC job remains a source/Win32 compile check, not XP validation.

### Configure and build on Windows

From the repository root, using a 32-bit generator/toolchain:

```text
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The `-A Win32` option applies to Visual Studio generators. For MinGW, configure with an explicitly selected 32-bit toolchain file or compiler path instead.

The output includes `BiblioLED.exe` and `config/settings.ini`. The application icon is compiled into `BiblioLED.exe` from the repository's `port.ico` resource; no separate icon file is needed beside the executable.

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

The executable is written to `build/xp-x86-w64devkit/BiblioLED.exe`. The CI job also checks that it is a 32-bit PE executable with Windows GUI subsystem version 5.01, rejects selected post-XP imports/API-set DLLs, and publishes a disposable artifact containing the executable, settings file, a short PE inspection report, and test instructions. The artifact does not contain a separate `.ico` file because the icon is embedded in the executable. The latest successful run is [XP-targeted Win32 build #84](https://github.com/quilterrantdean/BiblioLED/actions/runs/43615074515), for commit `5d8daf42320c378c7443a11129b68aef41a3509e`. It passed compilation, SQLite-store, compression, retention and audit-log tests, PE32/i386/GUI 5.01 and import checks, and uploaded artifact `14121334721`. The matching [Core tests run](https://github.com/quilterrantdean/BiblioLED/actions/runs/43615074582) passed on Ubuntu and modern Win32.

These checks establish an XP-oriented build target, not proof that the application runs on XP. The current proof of concept uses daily automatic backups before the first loan-data mutation, separate compressed manual and restore/migration safety backups, and automatic retention of 7 daily, 4 weekly and 12 monthly snapshots. New backups use statically linked zlib 1.4 with configurable gzip/zlib format and level; the settings are in `config/settings.ini` and the dedicated backup-settings dialog. Existing uncompressed `.sqlite` backups remain restorable. It also protects against silently recreating a missing initialized database. New databases use schema version 1; existing loans-only databases are detected as version 0 and require explicit confirmation before a backed-up transaction adds the internal audit table. Loan changes and their audit events are committed atomically. Internal audit events are synchronised to `logs/audit.log`; incomplete final writes are recovered, duplicate events are avoided, and the active log is rotated at the configured size (default 5 MB). Disabling file logging pauses synchronisation but keeps internal SQLite auditing and allows mutations; re-enabling it synchronises the backlog. If enabled logging fails, mutations are blocked while reads remain available. The icon is embedded in the executable and table columns can be reordered and persisted in `config/settings.ini`. Guided recovery, later migrations, the general settings UI, and the complete application remain outstanding. Do not use it with the real application data.

## XP compatibility note

SQLite 3.54.0 changed its Windows implementation to use Slim Reader/Writer Locks, which are not available on Windows XP. SQLite's maintainer states that SQLite 3.53 and earlier continue to work on XP. The prototype therefore pins 3.53.4. See the [SQLite discussion](https://sqlite.org/forum/info/2f3abc1a9d346e4a779003d2e21204255bd7462c99396ffeee46025e22fe0f48).

This is a compatibility choice based on the SQLite API dependency change, not a substitute for executing the built application on Windows XP SP3.


## CI validation

The `Core tests` workflow has two jobs:

- `Core tests (Ubuntu)`: builds and tests the platform-independent SQLite store.
- `Win32 compile check (modern Windows SDK)`: compiles the 32-bit Win32 application and runs the SQLite store tests on a Windows runner.

Both jobs passed for the initial proof-of-concept branch head. The Windows runner is a modern Windows environment, not Windows XP.


## Missing-database protection

After the database has opened successfully for the first time, the application writes `config/database.initialized`. On later launches, if this marker exists but `data/biblioteca.sqlite` is missing, startup stops with an explanatory message. It does not create an empty replacement. The marker is retained when replacing application binaries and should be kept with the configuration directory.


## SQLite schema version and legacy migration

The application uses SQLite PRAGMA user_version to identify the schema:

- Version 0: the legacy loans(id, title, loan_date, type) table. The application can read it, but blocks changes until the user explicitly confirms migration.
- Version 1: the same loans table plus audit_events and its indexes. New databases are created directly at this version.

The version 0 to 1 migration first creates and validates a complete SQLite backup. It then uses a transaction to add the audit table/indexes, records a schema_migration event, sets user_version=1, and verifies the result. It does not rewrite loan rows. If the migration is declined or fails, the application remains read-only and the version 0 database remains unchanged.

Every loan create/edit/delete and its internal audit event are committed in the same SQLite transaction. The event stores the action, timestamp, loan ID, before/after values, and details. If the audit write fails, the loan change is rolled back. The internal audit event read API is a store-level facility; there is no in-app audit viewer. The external text-log synchronisation is implemented as described below.


## Embedded icon and persistent table layout

The repository keeps `port.ico` as the source asset, but the Win32 resource compiler embeds it in `BiblioLED.exe`. The portable package therefore contains no separate `.ico` file and the executable does not load one at run time.

The main list view enables native header drag-and-drop. The display order and width of each logical column are saved to `config/settings.ini` under `[columns]`:

```ini
[columns]
order=0,1,2,3,4
widths=320,100,130,130,55
```

The width values are in pixels. Layout is loaded at startup and saved after a column move, resize, or normal shutdown. An invalid settings file is preserved and is not overwritten merely to save table layout preferences.

## Backup formats, cadence and retention

New backups use the statically linked zlib 1.4 library. The supported wrappers are gzip (`.sqlite.gz`) and zlib (`.sqlite.zlib`); raw DEFLATE is not used. The format and compression level (0–9, default 6) are set in the application backup-settings dialog and stored in `config/settings.ini`:

```ini
[backups]
compression_format=gzip
compression_level=6
```

A backup is streamed through fixed-size buffers. Creation first makes a temporary SQLite Online Backup snapshot and checks it with `PRAGMA integrity_check`. The snapshot is then compressed, decompressed again to a temporary file, and checked a second time. Only after both checks succeed is the compressed file moved to its final name. A failed backup blocks the pending loan-data mutation.

Automatic backups are created at most once per local calendar day, immediately before the first actual loan-data change that day. A valid same-day snapshot is reused. Startup/shutdown events, other audit-only writes, settings changes, and read-only sessions do not trigger automatic backups.

Only automatic backups are subject to rolling retention: keep the latest 7 daily snapshots, then one per week for the following 4 weeks, then one per month for the following 12 months. Pruning runs only after a new automatic snapshot has been created and validated. Manual backups and safety backups made before restore or migration are stored separately and are never removed by this policy. Unrecognised or malformed filenames are preserved. Older backups using the former generic `BiblioLED_YYYYMMDD_HHMMSS.sqlite` naming scheme are also preserved because their original category cannot be identified safely; they can still be restored.

Restore accepts both compressed formats and older uncompressed `.sqlite` files. Compressed files are decompressed to a temporary SQLite database and checked for stream integrity, SQLite integrity and schema compatibility before the current database is backed up and replaced. Restore verification and the safety-copy rollback path remain in place.


## External audit log

SQLite `audit_events` remains the authoritative audit history. When diagnostics are enabled, committed events are synchronised to the UTF-8 append-only `logs/audit.log`. Timestamped `audit_*.log` archives are created when the configured size threshold (default 5 MB) would be exceeded. No archive is deleted automatically.

Stable event fingerprints prevent duplication when synchronisation is repeated or resumed after a crash. An incomplete final line in the active log is truncated and replayed from SQLite. If diagnostics are deliberately disabled, events continue to be stored in SQLite and file synchronisation pauses; the backlog is replayed when diagnostics are re-enabled. If diagnostics remain enabled but the log cannot be updated, database mutations are blocked while reads remain available. Press F5 after correcting the file-system problem to retry synchronisation and unlock mutations without restarting. Current internal event coverage includes loan create/edit/delete, schema migration, application startup/shutdown, and successful manual backup/restore operations. Settings, exports, warnings, errors, and failure events remain to be added. Recording startup in SQLite is the first database mutation in a normal session, so it triggers that session's automatic pre-mutation backup even when external file logging is disabled.
