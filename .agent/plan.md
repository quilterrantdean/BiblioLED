# Project plan

## Current status

**Stage:** the Win32/SQLite proof of concept now includes automatic/manual/safety backups with compressed restore and retention, explicit schema migration and missing-database protection, transactional SQLite auditing with recoverable rotating external logs, the embedded icon and persistent table columns, a searchable history window, general and backup settings dialogs, an About dialog, and a read-only two-area Statistics window and a user-facing CSV export of the complete history. Loan-date/expiry/quota rules and statistics calculations are in portable core modules with dedicated tests. Commit `02e3aa801d127fef3e0078271c8dcbecca7338f8` passed Linux core tests, modern Win32 compile/tests, and the XP-targeted w64devkit x86 build, tests, PE32/XP-subsystem inspection and package generation. XP test artifact ID: `14132896673`. Actual Windows XP SP3 runtime validation remains outstanding.

The application remains a proof of concept, not the complete replacement. The XP-targeted workflow builds with pinned w64devkit x86, runs both SQLite-store and external-log tests, checks PE32/i386, GUI subsystem 5.01 and imports, and publishes a disposable test artefact. The user's real database has not been opened, copied, migrated, or modified.

The `development` branch was created from `main` and now contains the first C++/Win32/SQLite prototype. All development commits remain confined to this branch.

## Agreed product direction

BiblioLED remains a personal loan-recording and loan-management utility. It is not intended to manage a public library, catalogue a collection, or track reading progress.

The main window remains an operational overview of active loans and useful current information. The full history remains a separate window. The visual direction is a clean, compact Windows utility inspired by NirSoft and small, efficient native tools.

## Work phases

### Phase 1 — Requirements and technical baseline

- [x] Define the product scope and broad interface direction.
- [x] Define the data-integrity, backup, restore, and migration principles.
- [x] Define portable storage, settings, and diagnostic-log requirements.
- [x] Define architectural separation, testing expectations, and branch safety.
- [x] Establish the high-level UI direction and primary window behaviour. Remaining visual details will be decided during implementation.
- [x] Resolve the main loan-list behaviour for future-dated and expired records.
- [x] Close high-level product decisions; defer low-impact implementation details to the development work.

### Phase 2 — Technology proof of concept

- [x] Establish C++ / native Win32 / SQLite as the leading proof-of-concept direction.
- [x] Establish C++11 and CMake for the initial prototype.
- [x] Pin SQLite 3.53.4 for the XP compatibility baseline; newer releases require revalidation.
- [x] Add the first native Win32 window, list view, loan dialog, and SQLite CRUD implementation.
- [x] Add isolated SQLite store tests and GitHub Actions jobs for Linux and Win32.
- [x] Select and document the initial XP-targeted compiler baseline: w64devkit x86 (XP-compatible x86 package; Pentium 4/SSE2 requirement).
- [x] Validate the pinned w64devkit build, SQLite store test, and PE headers in the dedicated XP-targeted CI job.
- [ ] Decide whether the current w64devkit baseline is final after real XP runtime validation.
- [ ] Build and run the Win32 application on Windows XP SP3.
- [ ] Measure startup time, executable size, memory use, and responsiveness.
- [ ] Decide whether Free Pascal still merits a direct comparison after the C++ XP test.

### Phase 3 — Foundation

- [x] Establish the initial project structure and CMake build configuration.
- [ ] Implement the portable path model.
- [x] Implement INI settings loading, validation, and persistence for the current prototype.
- [x] Implement SQLite schema versioning (PRAGMA user_version): new databases use version 1; the existing loans-only schema is recognised as version 0.
- [x] Implement the explicit, backed-up, transactional version 0 to 1 migration. It adds the internal audit table and indexes without rewriting loan rows.
- [x] Implement one automatic SQLite Online Backup snapshot before the first loan-data mutation per local calendar day; audit-only events and settings changes do not trigger it. Reuse a valid same-day snapshot and fail closed if a new snapshot cannot be created.
- [x] Implement compressed manual backups and a confirmed restore workflow with a separately classified, verified pre-restore safety copy; keep legacy uncompressed `.sqlite` backups restorable.
- [x] Detect a missing database after initialisation and refuse silent recreation.
- [ ] Implement guided missing/corrupt-database recovery and reporting.
- [x] Store internal audit events in the same SQLite transaction as each loan create/edit/delete; include before/after values and migration events.
- [x] Synchronise internal audit events to the external append-only UTF-8 log using stable event fingerprints; recover an incomplete final line and rotate archives at the configured size.
- [x] When file logging is disabled, retain internal audit events in SQLite and queue external synchronisation; when re-enabled, synchronise the backlog. If enabled logging fails, allow reads but block further database mutations until recovery; F5 retries synchronisation without requiring an application restart.
- [x] Record application startup/shutdown and successful manual backup/restore operations as internal audit events, using the existing transactional persistence and external-log synchronisation path.
- [ ] Extend audit coverage beyond the implemented backup-settings change event to other settings, exports, warnings, errors, and failures.
- [x] Separate audit-only writes (including startup/shutdown and settings changes) from loan-data backup triggers.
- [x] Implement the approved automatic-backup retention policy: 7 daily, 4 weekly, and 12 monthly snapshots. Only recognised automatic backups are pruned; manual and restore/migration safety backups are preserved. Unknown filenames are preserved.
- [x] Integrate statically linked zlib 1.4 for streaming gzip and zlib wrappers. Verify compressed round-trips and SQLite integrity; keep `.sqlite` backups compatible.
- [x] Add backup settings for format (`gzip` or `zlib`) and compression level (0–9, default 6) to `config/settings.ini` and a dedicated Win32 dialog. Changes are applied immediately to the open `LoanStore`, without restarting; a valid same-day automatic snapshot remains reused.
- [x] Use native filesystem path separators for generated backup paths (`\\` on Windows, `/` on POSIX), avoiding mixed separators in messages and file operations.

The live-settings and native-path corrections pass Linux Core tests, modern Win32 compile/tests, and the XP-targeted w64devkit x86 build/tests. Latest validated commit: `1a9170d51ce7ea80fac5cedfbcf8c403b54f4e69`; XP test artifact ID: `14124735304`. Physical Windows XP SP3 runtime testing remains outstanding.

### Phase 4 — Application functionality and interface

- [x] Extract date parsing, calendar-day expiry, operational visibility, quota windows and remaining-quota calculation into portable core code; test these rules without the GUI.
- [ ] Implement the main active-loans overview.
- [x] Implement the separate searchable/filterable history window; show all records including expired/future entries, filter by title/type/date/ID, and support edit/delete.
- [x] Implement revised statistics: annual evolution/comparison, daily activity, weekday/hour patterns, and mean/median intervals between distinct active dates; no progress or streak tracking.
- [x] Implement the general settings UI for limits, loan duration, diagnostics and log rotation; changes apply on next launch as specified by D-016.
- [x] Implement the About dialog with the embedded icon, version, build timestamp and XP target.
- [x] Add a dedicated backup-settings dialog for compression format and level, persisted in `config/settings.ini`.
- [x] Embed the existing application icon as a Win32 resource in `BiblioLED.exe`; do not ship or require a separate `.ico` file.
- [x] Make the main table columns reorderable by dragging their headers.
- [x] Persist column display order and individual widths in `config/settings.ini`; invalid settings files are preserved and are never overwritten by layout saving.

### Phase 5 — Verification and delivery

- [ ] Continue expanding automated coverage for recovery, migration, settings persistence and UI workflows. Existing loan-rule, statistics, backup/compression, retention, audit and store tests run on synthetic/isolated data; the latest Linux, modern Win32 and XP-targeted CI runs pass.
- [ ] Add static analysis and dependency checks.
- [x] Configure the initial GitHub Actions core-test workflow for pushes and pull requests to `development`.
- [x] Add a separate w64devkit x86 XP-targeted build workflow and reproducible CMake preset.
- [x] Confirm the w64devkit XP-targeted workflow passes and publish a downloadable test build.
- [ ] Expand CI to cover the full test suite.
- [ ] Validate the application and tests on Windows XP as well as newer systems.
- [ ] Produce a portable 32-bit ZIP distribution.
- [ ] Document the build and release process.

## Remaining implementation questions

- Confirm w64devkit x86 as the final compiler/toolchain after the Windows XP SP3 runtime test.
- Exact external-log synchronisation behaviour when file logging is deliberately disabled.
- Low-impact UI details (final column widths, menu commands, and progressive history-loading threshold for very large histories).

Resolve these during implementation unless they materially change stored data or the agreed user experience.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
