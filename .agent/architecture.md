# Architecture

This document describes the target architecture and the C++/Win32/SQLite implementation now being built on development. The application remains a proof of concept until actual Windows XP SP3 execution and the remaining safety/UI work are complete.

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
  BiblioLED.exe  (application icon embedded as a Win32 resource)
  config/
    settings.ini  (application settings and persistent table layout)
  data/
    biblioteca.sqlite
    backups/
  exports/
  logs/
  resources/
    port.ico
```

All paths are resolved from the executable directory. The existing application icon is compiled into BiblioLED.exe as a Win32 resource; the portable distribution does not need a separate icon file. The loan table supports header drag-and-drop; its display order and per-column widths are persisted in config/settings.ini. Program updates must not overwrite user data, settings, backups, exports, or logs.

## Database safety model

- SQLite is the authoritative store.
- Database schema version is stored in SQLite PRAGMA user_version.
- Version 0 is the existing loans-only schema; it remains readable but read-only until the user confirms migration.
- Version 1 adds audit_events and its indexes. New databases are created directly at this version.
- The version 0 to 1 migration creates and validates a full backup first, then adds the audit table and indexes transactionally. It does not rewrite or discard loan records.
- Do not silently create a fresh database when an expected database cannot be opened.
- A small initialisation marker in `config/database.initialized` distinguishes first launch from a database that has gone missing; if the marker exists but the database does not, stop and explain rather than creating an empty database.
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

Internal audit events are now stored in audit_events within the same SQLite transaction as the associated loan create/edit/delete. Events retain the action, timestamp, loan ID, before/after values, and details. If the event insert fails, the loan operation rolls back as well. The schema migration itself records a schema_migration event. A read API is available to retrieve these events.

The external append-only UTF-8 text log is not yet implemented. The intended next step is to synchronise committed internal events to that log and recover missing file entries after an interrupted write. The exact behaviour when file logging is disabled remains open; internal audit records are retained in SQLite regardless.

## Main UI and secondary windows

- Main window: active loans and current operational information; do not make the full history the main view.
- History: separate window; real-time filters; keep it responsive for long histories.
- Statistics: general/evolution and usage-pattern views; no reading-progress or streak tracking.
- About: existing icon, application name, version, short description, and build date.
- Appearance: compact native Windows utility, prioritising legibility and functional density over decoration.
- Main table columns are reorderable by dragging headers. Store display order and widths per logical column in the existing settings.ini and restore them at startup. Do not rewrite an invalid settings file merely to save UI preferences.


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

The current implementation is deliberately a proof of concept, not the complete application. The store makes and validates one automatic backup before the first data mutation in each session, blocks changes if this fails, supports separate manual backups, and restores a validated backup only after creating a second safety copy of the current database. A marker prevents silent recreation of a missing initialized database. Schema version 1 and an explicit, backed-up version 0 to 1 migration are implemented. Loan changes and internal audit events commit atomically. Guided recovery, external text-log synchronisation, further migrations, history, statistics, settings UI, and other application features remain outstanding.

SQLite 3.53.4 is fetched as an amalgamation and statically linked for Windows builds. It is pinned because SQLite 3.54.0 introduced a Windows SRW-lock dependency that is not available on Windows XP. The pin must be revisited only after target-OS testing.
