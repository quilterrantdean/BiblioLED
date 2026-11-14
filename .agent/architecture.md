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
```

All paths are resolved from the executable directory. The existing application icon is compiled into BiblioLED.exe as a Win32 resource; the portable distribution does not need a separate icon file. Keep port.ico in the source repository as the resource input. The loan table supports header drag-and-drop; its display order and per-column widths are persisted in config/settings.ini. Program updates must not overwrite user data, settings, backups, exports, or logs.

## Database safety model

- SQLite is the authoritative store.
- Database schema version is stored in SQLite PRAGMA user_version.
- Version 0 is the existing loans-only schema; it remains readable but read-only until the user confirms migration.
- Version 1 adds audit_events and its indexes. New databases are created directly at this version.
- The version 0 to 1 migration creates and validates a full backup first, then adds the audit table and indexes transactionally. It does not rewrite or discard loan records.
- Do not silently create a fresh database when an expected database cannot be opened.
- A small initialisation marker in `config/database.initialized` distinguishes first launch from a database that has gone missing; if the marker exists but the database does not, stop and explain rather than creating an empty database.
- Test databases are always isolated from the user's real database.
- Use SQLite's Online Backup API to create a consistent snapshot in a temporary file.
- Create at most one automatic snapshot per local calendar day, immediately before the first actual loan-data mutation that day. Reuse a valid same-day snapshot. Audit-only events (including startup/shutdown), settings changes, and read-only launches do not trigger automatic snapshots.
- Automatic, manual, and safety backups have separate filename namespaces: `BiblioLED_auto_*`, `BiblioLED_manual_*`, and `BiblioLED_safety_*`.
- Retain automatic backups using the approved rolling schedule: the latest 7 daily snapshots, one per week for the following 4 weeks, then one per month for the following 12 months. Prune only recognised automatic backup files, and only after a new automatic snapshot has been created and validated. Unknown or malformed names are preserved.
- Manual backups remain until the user deletes them. Safety backups made before restore or migration are also preserved and are never removed by automatic retention.
- Compress new backups using statically linked zlib 1.4. The supported standard wrappers are gzip (`.sqlite.gz`) and zlib (`.sqlite.zlib`); the format and level (0–9, default 6) are selected in the backup settings and stored in `config/settings.ini`. Do not use raw DEFLATE.
- Stream compression/decompression with a fixed-size buffer; never load the complete database into memory.
- Backup creation sequence: SQLite snapshot -> SQLite integrity check -> compression -> decompression round-trip -> second SQLite integrity check -> move to final filename. A failed step must not publish a final backup or allow the pending loan mutation.
- Restore accepts the new compressed formats and legacy uncompressed `.sqlite` files. Decompress to a temporary file when necessary, validate SQLite integrity and schema compatibility, create and validate a safety backup of the current database, and only then perform the restore. Verify the restored database and attempt rollback from the safety backup on failure.
- A backup is not considered valid merely because a file exists; validate the complete stream and SQLite integrity/schema.

The backup settings dialog currently exposes compression format and level. The general settings window remains outstanding. See confirmed decisions D-019, D-020, D-057 and D-058.

## Audit logging model

Internal audit events are now stored in audit_events within the same SQLite transaction as the associated loan create/edit/delete. Events retain the action, timestamp, loan ID, before/after values, and details. If the event insert fails, the loan operation rolls back as well. The schema migration itself records a schema_migration event. A read API is available to retrieve these events.

The external append-only UTF-8 text log is implemented at `logs/audit.log`, with timestamped `audit_*.log` archives. Each line includes an event fingerprint derived from its durable content and identity. Synchronisation scans the active log and archives to avoid duplicates, repairs an incomplete final line in the active log after an interrupted write, and appends any missing SQLite events. The active file is rotated before an append would exceed the configured maximum (default 5 MB); archives are never deleted automatically. Internal events currently cover loan create/edit/delete, schema migration, application startup/shutdown, and successful manual backup/restore operations. A generic `RecordAuditEvent` store operation writes operational events transactionally without invoking the loan-data backup gate. Consequently startup/shutdown, settings-change, and other audit-only events do not create automatic database snapshots. They continue to obey the external-log synchronisation rules.

When file logging is deliberately disabled, internal SQLite audit events continue to be recorded and external synchronisation pauses; the backlog is replayed when logging is re-enabled. When logging is enabled but synchronisation fails, the application remains readable but blocks further database mutations until the log is healthy again. The user can press F5 to retry synchronisation after correcting the underlying file-system problem, without restarting the application.

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

The current implementation is deliberately a proof of concept, not the complete application. The store makes and validates one automatic compressed snapshot before the first loan-data mutation per local calendar day, supports separately retained compressed manual and safety backups, and restores compressed or legacy uncompressed backups only after creating a second safety copy of the current database. A marker prevents silent recreation of a missing initialized database. Schema version 1 and an explicit, backed-up version 0 to 1 migration are implemented. Loan changes and internal audit events commit atomically. External audit events are synchronised to a recoverable rotating UTF-8 log with a durable backlog. Current internal audit coverage includes loan create/edit/delete, schema migration, application startup/shutdown, successful manual backup/restore operations, and changes to backup compression settings. The backup compression settings dialog records its changes. Other settings, exports, warnings, errors, and failure events remain to be added. The application icon is embedded in the executable, and main-table order/width preferences persist in settings.ini. Guided recovery, further migrations, history, statistics, the general settings UI (beyond backup compression options), and other application features remain outstanding.

SQLite 3.53.4 is fetched as an amalgamation and statically linked for Windows builds. It is pinned because SQLite 3.54.0 introduced a Windows SRW-lock dependency that is not available on Windows XP. The pin must be revisited only after target-OS testing.
