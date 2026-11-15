# Building BiblioLED

## Project status and scope

BiblioLED is being rewritten as a native C++11 / Win32 / SQLite application. The intended Windows target is **32-bit Windows XP SP3**, including Pentium 4/SSE2 hardware.

The current application is a proof of concept, not a supported release. It already implements loan CRUD, history/statistics, CSV export, settings, compressed backup/restore, retention, SQLite schema migration, guided recovery, and internal/external audit logging. The safety and CI work is substantially more complete than the GUI's target-machine validation.

**Do not use the current proof of concept with the real BiblioLED database.** Automated tests create isolated synthetic databases. The downloadable XP artefact contains no database.

A successful XP-targeted build, PE-header inspection, and automated tests do not prove that the application runs correctly on a physical Windows XP SP3 installation. The recovery wizard, Portuguese resource glyphs, dialogs, and general UI still require a recorded run on the target machine.

## Toolchains and dependencies

### XP-targeted Windows build

The pinned baseline is the **x86 edition of w64devkit 2.10.0**. The expected compiler target is `i686-w64-mingw32`; the x86 package requires SSE2 and is intended to support Windows XP. This matches the Pentium 4 target.

The CI workflow downloads the upstream release archive and verifies SHA-256 before extraction:

- w64devkit x86 2.10.0: `513ff7fc571cf6764a5aedbca92f43c513f8d01085c65df5133a82db13a7e63d`

The XP CMake preset uses CMake 3.28 or later (preset schema version 6). The w64devkit package used by CI contains the CMake executable and build tools used by that workflow.

### Windows dependencies

At first Windows configure, CMake downloads and verifies these source archives:

- SQLite amalgamation 3.53.4, statically linked. SHA3-256: `628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e`.
- zlib 1.4, statically linked. The source is fetched from the upstream `madler/zlib` v1.4 GitHub Release asset, not the zlib.net fossil endpoint. SHA-256: `02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584`.

SQLite 3.53.4 is pinned because the Windows implementation in SQLite 3.54.0 introduced a dependency on Slim Reader/Writer Locks, which Windows XP does not provide. Do not update SQLite without a deliberate XP compatibility review.

The zlib URL was moved to the official upstream GitHub Release asset after the zlib.net fossil endpoint returned inconsistent contents and repeated SHA-256 mismatches in CI. The published upstream digest is retained; the hash check has not been weakened.

### Linux development/test host

On Debian/Ubuntu, install the system development packages:

```sh
sudo apt-get update
sudo apt-get install -y cmake g++ make libsqlite3-dev zlib1g-dev
```

The portable Linux build uses the host's SQLite and zlib development packages. The Windows build uses the pinned SQLite amalgamation and pinned static zlib source.

## Build and test

### Linux core tests

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The Linux suite covers the portable core, SQLite store, backup compression, and retention. It does not build the Win32 UI or the Windows-only external audit-log test.

### Win32 build with Visual Studio

Use a 32-bit Visual Studio generator:

```powershell
cmake -S . -B build -A Win32 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`-A Win32` is a Visual Studio generator option. It is not used with MinGW generators. The modern-Windows CI job compiles the Win32 application and runs the Windows tests, but it is not an XP runtime test.

### Windows XP target with w64devkit x86

Open the x86 w64devkit shell and change to the repository root. Check that the compiler is the expected 32-bit target:

```sh
gcc -dumpmachine
```

Expected output:

```text
i686-w64-mingw32
```

Then configure, build, and run the tests:

```sh
cmake --preset xp-x86-w64devkit
cmake --build --preset xp-x86-w64devkit --parallel
ctest --preset xp-x86-w64devkit
```

The executable is written to:

```text
build/xp-x86-w64devkit/BiblioLED.exe
```

The XP workflow additionally checks that the executable is PE32/i386, uses Windows GUI subsystem 5.01, and does not import a selected list of post-XP APIs or API-set DLLs. It packages a disposable test ZIP containing the executable, example settings, PE inspection report, and test instructions.

### CI test matrix

The `Core tests` workflow has three jobs:

- Ubuntu: portable core/store/backup tests.
- Modern Windows: Win32 compilation plus SQLite-store and Windows audit-log tests.
- Cppcheck: version-gated Cppcheck 2.19.0 analysis of selected portable C++11 sources and tests.

The separate `XP-targeted Win32 build` workflow uses pinned w64devkit x86 2.10.0, builds the application and tests, runs the seven XP-targeted tests, inspects the PE executable/imports, and uploads the disposable package.

The Windows/XP test set comprises:

1. `loan_csv_export_tests`
2. `loan_rules_tests`
3. `loan_statistics_tests`
4. `loan_store_tests`
5. `backup_compression_tests`
6. `backup_retention_tests`
7. `audit_log_tests` (Windows only)

### Latest validated build

The latest validated product-code commit is `909628715a09b9fa9106124c3c2a5b9d1a30e877`. Its validation passed:

- [Core tests run 43766820549](https://github.com/quilterrantdean/BiblioLED/actions/runs/43766820549): Ubuntu, modern Win32 build/tests, and Cppcheck 2.19.0 passed, including tests for rejecting incompatible look-alike legacy schemas without changing their records.
- [XP-targeted build run 43766820536](https://github.com/quilterrantdean/BiblioLED/actions/runs/43766820536): w64devkit x86 2.10.0, all seven tests, PE32/i386, GUI subsystem 5.01, and import checks.

The disposable artefact is [14182637061](https://github.com/quilterrantdean/BiblioLED/actions/runs/43766820536), `BiblioLED-XP-PoC`:

- ZIP size: 1,502,035 bytes
- ZIP SHA-256: `d80f22b9b2b31edd8cf9be528954e2c0503f54058fb1885453f756fa69fa577e`
- `BiblioLED.exe` SHA-256: `3129b7ffeafbd82ecf49ff2fbc13dba243cf83f13e2a8122cf498846a47f8d28`

The archive contains exactly four files: `BiblioLED.exe`, `config/settings.ini`, `pe-inspection.txt`, and `LEIA-ME-TESTE.txt`. It does not contain a database or a separate icon file; the application icon is embedded in the executable. The Portuguese test guide is UTF-8 with BOM for Windows XP Notepad.

This ZIP is a disposable CI test artefact, not a normal portable distribution or release.

## Portable directory layout and settings

The application stores its data beside the executable. The expected structure is:

```text
BiblioLED/
  BiblioLED.exe
  config/
    settings.ini
    database.initialized
  data/
    biblioteca.sqlite
    backups/
  exports/
  logs/
    audit.log
    audit_*.log
```

`config/settings.ini` is supplied as an example. The icon is embedded in `BiblioLED.exe`; `port.ico` remains in the source repository as the resource input.

The general settings dialog persists loan limits, loan duration, diagnostics, and audit-log size. General settings take effect on the next launch. The separate backup-settings dialog applies compression format and level immediately to the open store.

On Windows, application/store paths are passed as UTF-8 and converted to UTF-16 for file-system operations. The INI file is read/written using wide-character Win32 APIs. Required directories are validated on startup; if a required path is a file rather than a directory, startup stops before opening the database.

## SQLite schema, migration, and recovery

The schema version is stored in `PRAGMA user_version`. The legacy `loans` table is accepted only in its exact four-column form: `id INTEGER PRIMARY KEY` with `title`, `loan_date`, and `type` declared `TEXT NOT NULL`. Look-alike tables with a text ID, no integer primary key, or unsupported extra columns are rejected without modification:

- Version 0 is the legacy `loans(id, title, loan_date, type)` table.
- Version 1 is the same loans table plus `audit_events` and its indexes.

New databases are created directly at version 1. An existing version-0 database can be read, but loan changes are blocked until the user explicitly confirms migration. Migration first creates and validates a complete safety backup, then adds the audit table/indexes and records the migration in one SQLite transaction. The existing loan rows are not rewritten. A failed migration rolls back its partial DDL and leaves the database at version 0.

After first successful initialisation, `config/database.initialized` distinguishes a missing database from first launch. If the marker exists but the database is missing, startup does not create an empty replacement. If the existing database cannot be opened, startup offers guided recovery from a user-selected backup.

The recovery flow is explicit, not automatic. The candidate backup is decompressed and checked for stream integrity, SQLite integrity, and a supported BiblioLED schema. It is copied into a staging database and checked again. The existing database and SQLite `-journal`, `-wal`, and `-shm` sidecars are preserved before the staged database is installed. The selected backup is not consumed. If recovery fails, the application attempts to restore the original files and reports any incomplete rollback. All automated recovery tests use synthetic databases and temporary directories.

## Backups, compression, and retention

Loan-data changes create at most one automatic snapshot per local calendar day, immediately before the first actual loan-data mutation of that day. A valid same-day snapshot is reused. Audit-only events, startup/shutdown events, settings changes, and read-only sessions do not create automatic loan-data snapshots.

Backups use statically linked zlib 1.4 and standard wrappers:

- gzip: `.sqlite.gz`
- zlib: `.sqlite.zlib`

The compression format and level (0–9, default 6) are set in the backup-settings dialog and stored in `config/settings.ini`. Raw DEFLATE is not used. Older uncompressed `.sqlite` backups remain restorable.

The backup pipeline streams through fixed-size buffers. It creates a temporary SQLite Online Backup snapshot, runs `PRAGMA integrity_check`, compresses the snapshot, decompresses it again to a temporary database, and checks integrity/schema again. Only a fully validated result is installed under its final backup name. Completed compressed/decompressed output is flushed through the C runtime and then synchronised to the operating system (`FlushFileBuffers` on Windows, `fsync` on POSIX) before the file is closed and accepted. On Windows, final backup moves use `MoveFileExW` with write-through semantics.

Automatic retention keeps the latest 7 daily snapshots, one per week for the following 4 weeks, then one per month for the following 12 months. Only recognised automatic backups are pruned, and pruning occurs after a new automatic snapshot has been validated. Manual backups and safety backups before restore/migration are separate categories and are never removed by automatic retention. Unknown/malformed filenames are preserved.

Manual backups use the `BiblioLED_manual_*` namespace. Restore/migration safety backups use `BiblioLED_safety_*`. Automatic backups use `BiblioLED_auto_*`.

Restore accepts `.sqlite.gz`, `.sqlite.zlib`, and legacy uncompressed `.sqlite`. It validates the selected source before creating a safety copy of the current database. After replacement, the restored database is checked; if verification fails, the store attempts rollback from the safety copy. A rollback failure is reported, and the database connection is not reused in an uncertain transaction state.

The file-data flush is not a universal promise against every power-loss scenario: POSIX parent-directory metadata is not separately fsynced after rename, and abrupt power-loss/process-interruption coverage remains outstanding.

## Audit logging

SQLite `audit_events` is the authoritative audit history. Loan create/edit/delete and their audit events are committed in the same transaction. Current internal event coverage includes loan CRUD, schema migration, application startup/shutdown, successful manual backup/restore, general/backup settings changes, and CSV export. Warning/error/failure-event coverage remains incomplete.

When external diagnostics are enabled, committed events are synchronised to `logs/audit.log`, with timestamped `audit_*.log` archives. Stable fingerprints prevent duplicate replay. An incomplete final line in the active log can be truncated and replayed from SQLite; an incomplete archived log is rejected unchanged. Archives are not automatically deleted.

The scanner reads files with a fixed 64 KiB buffer and bounded per-line parser state; it does not keep complete archive contents in memory. However, its de-duplication set retains one fingerprint per distinct event found across the active log and archives, so total memory still grows with the number of retained historical events.

If external diagnostics are deliberately disabled, SQLite auditing continues and external synchronisation pauses; the backlog is replayed when diagnostics are re-enabled. If diagnostics are enabled but synchronisation fails, reads remain available but further database mutations are blocked until logging recovers. The recovery command is **Ficheiro > Tentar sincronizar registo de auditoria...**. It is not bound to F5.

## Safe manual XP test

Use a fresh disposable folder, for example `C:\Temp\BiblioLED_ação`. Do not copy the real database or replace the current installation.

The package's `LEIA-ME-TESTE.txt` contains the detailed manual test sequence. It includes:

- first launch and creation of a synthetic database;
- creating, editing, deleting, and viewing fictitious loan records;
- history filters, statistics, CSV export, and settings persistence;
- gzip/zlib format and compression-level changes;
- manual backup and restore from a test backup;
- startup recovery from a deliberately missing/corrupt test database;
- Unicode paths such as `C:\Temp\BiblioLED_ação`;
- required-directory collision behaviour.

Run the tests in a separate disposable directory. Preserve the real installation and data unchanged.

## Compatibility and remaining limitations

The target PE is 32-bit i386 with Windows GUI subsystem 5.01. CI rejects selected post-XP APIs and API-set DLL imports, and the compiler package is the XP-compatible x86 w64devkit build. This is build/import compatibility evidence, not a substitute for running the executable on Windows XP SP3.

Before considering a normal portable distribution, the project still needs:
- a reproducible manual Windows XP SP3 runtime test, including resource glyphs, recovery wizard, and dialogs;
- failure injection for the `ROLLBACK` statement itself and abrupt process/power-loss interruption;
- broader audit events for warnings, errors, and failures;
- later schema migration work as product requirements evolve;
- a final release/build process and portable ZIP review.

Do not use this proof of concept with the user's real application data.
