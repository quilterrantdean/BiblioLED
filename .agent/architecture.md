# Architecture

This document describes the target architecture and the C++/Win32/SQLite implementation now being built on development. The application remains a proof of concept until the remaining safety work is complete and the user's manual target-system test is documented sufficiently to reproduce its conditions. The user reports having tested it; exact build and scenarios remain unrecorded.

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
- Single-instance coordination uses a named Win32 mutex derived from the executable directory. If Windows cannot create the mutex, startup fails closed rather than opening the database without the single-instance guarantee.
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
- A small initialisation marker in `config/database.initialized` distinguishes first launch from a database that has gone missing. If the marker exists but the database does not, never create an empty database; offer the guided recovery flow instead.
- If the marker exists but the database is missing, or if an existing database cannot be opened, startup offers explicit recovery from a user-selected backup. The candidate is decompressed and checked for full SQLite integrity and a supported BiblioLED schema, copied to a staging database and validated again before installation. Existing database files and any `-journal`, `-wal`, or `-shm` sidecars are renamed together and retained; the chosen backup is never consumed. An invalid candidate does not alter the destination. All SQLite-backed temporary basenames (snapshot, verification, decompression and recovery staging) are skipped if the main file or any `-journal`, `-wal`, or `-shm` sidecar already exists; cleanup removes the temporary file group created by the operation. A corrupt automatic backup and any sidecars are moved out of the automatic-retention namespace as one group; if a partial move fails, already moved files are rolled back. Recovery is never automatic.
- Test databases are always isolated from the user's real database.
- Use SQLite's Online Backup API to create a consistent snapshot in a temporary file.
- Create at most one automatic snapshot per local calendar day, immediately before the first actual loan-data mutation that day. Reuse a valid same-day snapshot. Audit-only events (including startup/shutdown), settings changes, and read-only launches do not trigger automatic snapshots.
- Automatic, manual, and safety backups have separate filename namespaces: `BiblioLED_auto_*`, `BiblioLED_manual_*`, and `BiblioLED_safety_*`.
- Retain automatic backups using the approved rolling schedule: the latest 7 daily snapshots, one per week for the following 4 weeks, then one per month for the following 12 months. Prune only recognised automatic backup files, and only after a new automatic snapshot has been created and validated. Unknown or malformed names are preserved.
- Build filesystem paths with the platform-native separator (`\\` on Windows, `/` on POSIX) even though Win32 file APIs accept both slash styles; do not emit mixed separators in user-visible backup paths.
- The store's Windows path inputs are UTF-8. Existence checks convert to UTF-16 and use `GetFileAttributesW`; the settings INI is read/written through wide-character Win32 file APIs so non-ASCII executable-directory paths work with the portable layout. At startup, each required portable directory is checked as an actual directory (not merely an existing path); a file/directory collision or creation failure stops startup before the database is opened.
- Manual backups remain until the user deletes them. Safety backups made before restore or migration are also preserved and are never removed by automatic retention. Historical backups created by older versions with generic `BiblioLED_YYYYMMDD_HHMMSS.sqlite` names are also left untouched: their original purpose cannot be reliably distinguished, and they remain restorable.
- Compress new backups using statically linked zlib 1.4. The supported standard wrappers are gzip (`.sqlite.gz`) and zlib (`.sqlite.zlib`); the format and level (0–9, default 6) are selected in the backup settings and stored in `config/settings.ini`. Do not use raw DEFLATE. Changes made in the backup-settings dialog are applied to the open `LoanStore` immediately; they affect the next backup created without restarting. A valid automatic snapshot already created for the current day remains reused.
- Stream compression/decompression with a fixed-size buffer; never load the complete database into memory.
- Backup creation sequence: SQLite snapshot -> SQLite integrity check -> compression -> decompression round-trip -> second SQLite integrity check -> move to final filename. A failed step must not publish a final backup or allow the pending loan mutation.
- Restore accepts the new compressed formats and legacy uncompressed `.sqlite` files. Decompress to a temporary file when necessary, validate SQLite integrity and schema compatibility, create and validate a safety backup of the current database, and only then perform the restore. Verify the restored database and attempt rollback from the safety backup on failure.
- A backup is not considered valid merely because a file exists; validate the complete stream and SQLite integrity/schema.

The general settings dialog exposes the daily and monthly limits, rolling-window length, loan duration, diagnostic logging toggle, and external-log rotation threshold. These values are saved to `config/settings.ini` and intentionally remain pending until the next application launch (D-016). The dedicated backup dialog remains separate because compression changes apply immediately (D-057).

## Audit logging model

Internal audit events are now stored in audit_events within the same SQLite transaction as the associated loan create/edit/delete. Events retain the action, timestamp, loan ID, before/after values, and details. If the event insert fails, the loan operation rolls back as well. The schema migration itself records a schema_migration event. A read API is available to retrieve these events.

The external append-only UTF-8 text log is implemented at `logs/audit.log`, with timestamped `audit_*.log` archives. Each line includes an event fingerprint derived from its durable content and identity. Synchronisation scans the active log and archives to avoid duplicates, repairs an incomplete final line in the active log after an interrupted write, and appends any missing SQLite events. The active file is rotated before an append would exceed the configured maximum (default 5 MB); archives are never deleted automatically. Internal events currently cover loan create/edit/delete, schema migration, application startup/shutdown, successful manual backup/restore operations, and general/backup settings changes. A generic `RecordAuditEvent` store operation writes operational events transactionally without invoking the loan-data backup gate. Consequently startup/shutdown, settings-change, and other audit-only events do not create automatic database snapshots. They continue to obey the external-log synchronisation rules.

When file logging is deliberately disabled, internal SQLite audit events continue to be recorded and external synchronisation pauses; the backlog is replayed when logging is re-enabled. When logging is enabled but synchronisation fails, the application remains readable but blocks further database mutations until the log is healthy again. The Win32 UI exposes **Ficheiro > Tentar sincronizar registo de auditoria...** as the explicit recovery command. F5 is no longer used for main-window refresh or audit retry.

CSV export is generated by portable `csv_export.*` from all records returned by `LoanStore::GetAll`, using the legacy `Title;LoanDate;Type` columns, UTF-8 BOM, semicolon delimiter and correct quoting. The Win32 save dialog writes a temporary file in the destination folder and atomically replaces the destination only after a complete flush. Successful export is audited as `records_exported`; it is not a database backup.

## Business rules

Date parsing, local calendar-day arithmetic, loan expiry/visibility, quota-window boundaries and remaining-quota calculations live in the portable `biblioled_core` library (`loan_rules.*`). The Win32 interface calls these functions, and `loan_rules_tests` exercises them without starting the GUI or opening SQLite. Future-dated loans remain visible and reserve quota; expired loans stay in history.

Statistics calculations live in `loan_statistics.*`, operating on a supplied record vector without Win32 or database access. Annual evolution, daily calendar activity, weekday/hour distributions and full-history mean/median intervals are covered by `loan_statistics_tests`.

## Main UI and secondary windows

- Main window: active loans and current operational information; do not make the full history the main view.
- History: separate native Win32 dialog; all records including expired and future-dated loans; immediate case-insensitive filtering by title, type, date, and ID, plus a type selector. Search/display strings are cached at load time to avoid repeated conversions and database queries while typing. Edit/delete operations are routed through the existing LoanStore methods and transactional audit path; the history list is refreshed after a successful mutation.
- Statistics: read-only native Win32 dialog with two areas. General/evolution shows all-history totals and type distribution, first/last valid dates, annual totals and year-over-year counts/differences. Usage patterns shows selected-year daily activity and weekday/hour distributions. Mean/median gaps between distinct active dates use the full history (legacy metric), not the selected year. Invalid dates are counted and excluded from date-based metrics; no reading-progress or streak tracking.
- About: native Win32 dialog displaying the embedded application icon, application name, version, short description, compile date/time, and XP SP3 x86 target.
- Appearance: compact native Windows utility, prioritising legibility and functional density over decoration.
- Main table columns are reorderable by dragging headers. Store display order and widths per logical column in the existing settings.ini and restore them at startup. Do not rewrite an invalid settings file merely to save UI preferences.


## Confirmed UI interaction and refresh policy

These UI refinements are implemented:

- General and backup settings are under **Opções**; **Ficheiro** contains file/backup operations and the explicit audit-log synchronisation recovery command.
- Main-list refresh is event-driven: startup and successful create/edit/delete/restore only. There is no periodic polling, manual refresh command/button, or "Actualizado às ..." timestamp.
- History and statistics load a snapshot when opened. Statistics year changes recalculate from cached records; the redundant statistics refresh button and F5 reload path have been removed.
- History is resizable, enforces a minimum track size, supports header drag-and-drop and provides right-click Edit/Delete actions. Double-click-to-edit remains.
- Ctrl+N and Ctrl+H use a Win32 accelerator table translated by `TranslateAcceleratorW`. Enter invokes the list-view activation action, and Delete is handled as a list-view key notification.
- The resource script declares code page 65001, and MSVC C++ source compilation explicitly uses UTF-8. Statistics error strings now convert from UTF-8 correctly.
- F5 is not used for main-window refresh or audit retry. Audit-log recovery remains available from the explicit Ficheiro command.

The resource encoding must still be visually checked on the target system; compilation does not prove that all dialog glyphs render correctly.

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

The initial UI supports listing, creating, editing, and deleting loan records. The main window shows active/current and future-dated records, while the separate history dialog lists the complete loan table including expired records. The history dialog caches rows for immediate text/type filtering and delegates mutations to the main application window so the existing persistence and audit paths remain authoritative. The application reads the existing `loans(id, title, loan_date, type)` table shape and creates that exact table only for a new database. It uses the portable folder beside the executable.

Version 1 audit-schema validation now also requires the full `PRAGMA table_info` scan and statement finalisation to complete successfully; a partial metadata read is rejected instead of being treated as a valid schema. The external log scanner reads fixed-size chunks, commits event keys only from complete lines, and truncates only an incomplete tail in the active log; archived files remain immutable.

The current implementation is deliberately a proof of concept, not the complete application. The store makes and validates one automatic compressed snapshot before the first loan-data mutation per local calendar day, supports separately retained compressed manual and safety backups, and restores compressed or legacy uncompressed backups only after creating a second safety copy of the current database. A marker prevents silent recreation of a missing initialized database; guided startup recovery now validates a selected backup and preserves any unavailable/corrupt database and its SQLite sidecars before installing a staged copy. Schema version 1 and an explicit, backed-up version 0 to 1 migration are implemented. Loan changes and internal audit events commit atomically. External audit events are synchronised to a recoverable rotating UTF-8 log with a durable backlog. Current internal audit coverage includes loan create/edit/delete, schema migration, application startup/shutdown, successful manual backup/restore operations, and general/backup settings changes. Other settings, exports, warnings, errors, and failure events remain to be added. The application icon is embedded in the executable, and main-table order/width preferences persist in settings.ini. Further failure-injection coverage, later migrations, broader audit coverage, and other application features remain outstanding.

SQLite 3.53.4 is fetched as an amalgamation and statically linked for Windows builds. It is pinned because SQLite 3.54.0 introduced a Windows SRW-lock dependency that is not available on Windows XP. The pin must be revisited only after target-OS testing.


## Latest CI evidence

The implementation baseline at commit `923b8d38c61f22e93055fe112db1f780da61f5bd` passed:

- [Core tests run 43720771401](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771401): Ubuntu core tests and modern Windows SDK Win32 compile/store tests, including recovery rollback failure injection, invalid automatic-backup sidecar preservation, Unicode paths, and no-op mutation checks.
- [XP-targeted build run 43720771387](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771387): w64devkit x86 build, all 7 XP-targeted CTest tests, recovery rollback failure injection, PE32/i386 + GUI subsystem 5.01 + import inspection; artefact [14163967976](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771387), `BiblioLED-XP-PoC` (1,498,735 bytes), archive SHA-256 `b74248df2750df12c6b7b98262e646af65478cdd9f746055dfb25676c7b4602c`; executable SHA-256 `101cceab528567b3ac5ffb73ccbad1702a2ee352a63d4af6c33e52cdac7c482d`.

The artefact is a disposable test build, not a release. CI validates the file-move rollback path and startup checks, but does not replace runtime validation of the recovery wizard and GUI on a physical Windows XP SP3 system.

The user reports having tested the application manually, but the precise artefact, Windows build and scenarios have not yet been captured. CI checks are not themselves runtime evidence from a physical Windows XP SP3 installation.
