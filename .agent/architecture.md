# Proposed architecture

This is a target design, not a description of the current Python implementation. The exact language, compiler, build system, and file layout remain subject to the Windows XP proof of concept.

## Responsibilities and dependency direction

### 1. User interface

Responsibilities:
- Native Win32 windows, menus, dialogs, tables, keyboard shortcuts, and status information.
- Main operational view of active loans and availability.
- Separate full-history window, filters, and export commands.
- Settings, statistics, About, backup, and restore UI.
- Present progress and errors without implementing business rules.

The UI calls application services/use cases. It does not issue SQL directly.

### 2. Business rules

Responsibilities:
- Daily and rolling-window loan limits.
- Active-loan and expiry calculations.
- Next-availability calculations.
- Statistics and date/time rules.
- Validation of loan operations and settings.

This component must run and be tested without creating a GUI.

### 3. Persistence

Responsibilities:
- SQLite connections and transactions.
- Repositories/queries for loans and audit records.
- Schema versioning and migrations.
- Backup creation, integrity validation, and restore support.

The current database schema is the compatibility baseline: `loans(id, title, loan_date, type)`.

### 4. Infrastructure

Responsibilities:
- Portable path resolution based on the executable location, not the process working directory.
- Settings file loading, validation, and saving.
- Diagnostic log writing and archival.
- Single-instance coordination.
- File-system operations and application lifecycle support.

The application should not depend on AppData, registry installation, Windows services, or administrator privileges.

## Suggested dependency direction

- UI -> application services/use cases.
- Application services -> business rules and persistence interfaces.
- SQLite persistence implements the persistence interfaces.
- Infrastructure provides file, settings, logging, and lifecycle services.
- Business rules should not depend on Win32, SQLite implementation details, or GUI types.

Keep the design proportionate to a small desktop utility. Avoid unnecessary frameworks, elaborate plugin systems, and excessive abstraction.

## Portable folder layout

Suggested shape (final names may change with the chosen implementation):

```text
BiblioLED/
  BiblioLED.exe
  config/
    settings file
  data/
    biblioteca.sqlite
    backups/
  exports/
  logs/
  resources/
    port.ico
```

All paths are resolved from the executable directory. The existing application icon is retained. Program updates must not overwrite user data, settings, backups, exports, or logs.

## Database safety model

- SQLite is the authoritative store.
- Database schema has an explicit version.
- Migrations are versioned, transactional, backed up, validated, and require explicit confirmation.
- Do not silently create a fresh database when an expected database cannot be opened.
- Test databases are always isolated from the user's real database.
- Backups are standalone SQLite files, uncompressed, timestamped, and retained until the user deletes them.
- Before the first data mutation in each session, create one backup using SQLite's online-backup API; subsequent mutations in that session reuse it.
- Manual backups are separate timestamped files and do not replace the automatic per-session backup.
- Restore requires a compatible, integrity-checked source and explicit UI confirmation. Always create and validate a new backup of the current database immediately before restore.
- Validate the completed backup with `PRAGMA integrity_check` before allowing the pending mutation or restore.
- If backup creation or validation fails, reject the mutation/restore and keep the database available for reads.
- Keep both the selected source backup and the pre-restore safety copy unchanged.
- A backup is not considered valid merely because a file exists: validate its SQLite integrity.

## Audit logging model

The intended design must keep database changes and their audit events consistent. The proposed approach is to persist audit events within the same SQLite transaction as the associated data change, then synchronise them to the external append-only text log. This allows recovery if the process stops between the database commit and the file write.

The interaction between disabling file logs and retaining internal audit records remains an implementation detail to settle before coding.

## Main UI and secondary windows

- Main window: active loans and current operational information; do not make the full history the main view.
- History: separate window; real-time filters; keep it responsive for long histories.
- Statistics: general/evolution and usage-pattern views; no reading-progress or streak tracking.
- About: existing icon, application name, version, short description, and build date.
- Appearance: compact native Windows utility, prioritising legibility and functional density over decoration.


## Initial proof-of-concept implementation

The first C++ implementation slice is being introduced under:

```text
CMakeLists.txt
config/settings.ini
include/biblioled/loan_store.h
src/core/loan_store.cpp
src/win32/main.cpp
src/win32/resource.h
src/win32/dialogs.rc
tests/loan_store_tests.cpp
.github/workflows/core-tests.yml
```

The initial UI supports listing, creating, editing, and deleting loan records. It reads the existing `loans(id, title, loan_date, type)` table shape and creates that exact table only for a new database. It uses the portable folder beside the executable.

The current implementation is deliberately a proof of concept, not the complete application. The store now makes and validates one automatic backup before the first data mutation in each session, blocks changes if this fails, supports manual backups, and restores a validated backup only after creating a second safety copy of the current database. Missing-database recovery, schema migration, audit logging, history, statistics, and the settings dialog are not implemented yet.

SQLite 3.53.4 is fetched as an amalgamation and statically linked for Windows builds. It is pinned because SQLite 3.54.0 introduced a Windows SRW-lock dependency that is not available on Windows XP. The pin must be revisited only after target-OS testing.
