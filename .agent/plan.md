# Project plan

## Current status

**Stage:** the Win32/SQLite foundation now has session/manual backups, verified restore, missing-database protection, schema version 1 and explicit legacy migration, transactional internal auditing for loan changes, migration, lifecycle, and backup/restore operations, recoverable rotating external audit logs, an embedded executable icon, and persistent draggable table columns. The new operational-audit changes require fresh CI validation. Actual Windows XP SP3 runtime validation remains outstanding.

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
- [ ] Implement settings loading, validation, and persistence.
- [x] Implement SQLite schema versioning (PRAGMA user_version): new databases use version 1; the existing loans-only schema is recognised as version 0.
- [x] Implement the explicit, backed-up, transactional version 0 to 1 migration. It adds the internal audit table and indexes without rewriting loan rows.
- [x] Implement SQLite online backups before the first data mutation in each session, with integrity validation and fail-closed behaviour.
- [x] Implement manual SQLite backups and a confirmed restore workflow with a verified pre-restore safety copy.
- [x] Detect a missing database after initialisation and refuse silent recreation.
- [ ] Implement guided missing/corrupt-database recovery and reporting.
- [x] Store internal audit events in the same SQLite transaction as each loan create/edit/delete; include before/after values and migration events.
- [x] Synchronise internal audit events to the external append-only UTF-8 log using stable event fingerprints; recover an incomplete final line and rotate archives at the configured size.
- [x] When file logging is disabled, retain internal audit events in SQLite and queue external synchronisation; when re-enabled, synchronise the backlog. If enabled logging fails, allow reads but block further database mutations until recovery; F5 retries synchronisation without requiring an application restart.
- [x] Record application startup/shutdown and successful manual backup/restore operations as internal audit events, using the existing transactional persistence and external-log synchronisation path.
- [ ] Extend audit coverage to settings changes, exports, warnings, errors, and failures.

### Phase 4 — Application functionality and interface

- [ ] Implement the business rules independently of the GUI.
- [ ] Implement the main active-loans overview.
- [ ] Implement the separate searchable/filterable history window.
- [ ] Implement revised statistics.
- [ ] Implement settings and the About dialog.
- [x] Embed the existing application icon as a Win32 resource in `BiblioLED.exe`; do not ship or require a separate `.ico` file.
- [x] Make the main table columns reorderable by dragging their headers.
- [x] Persist column display order and individual widths in `config/settings.ini`; invalid settings files are preserved and are never overwritten by layout saving.

### Phase 5 — Verification and delivery

- [ ] Add exhaustive automated tests using isolated test data.
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
- Low-impact UI details (final column widths, menu commands, and progressive history-loading threshold).

Resolve these during implementation unless they materially change stored data or the agreed user experience.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
