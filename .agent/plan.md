# Project plan

## Current status

**Stage:** the C++11/Win32/SQLite proof of concept now includes guided recovery and rollback coverage, Unicode-safe paths, strict SQLite metadata/integrity checks, exact legacy-loans and version-1 audit-table schema contracts, checked SQL bindings/finalisation, guarded transaction rollback, durable backup-file flushing, click-to-sort in the main and history loan lists, diagnostic audit coverage for application warnings, errors and failures, and deterministic handling of explicit SQLite ROLLBACK failures, and recovery following abrupt process termination. Latest validated product-code head `738c1d07a1a8256a80a78c5cc03135576c3c4aba` on `development` passed the complete CI matrix:

- [Core tests run 43896578995](https://github.com/quilterrantdean/BiblioLED/actions/runs/43896578995): Ubuntu core/store tests, modern Windows SDK Win32 build/store tests, and Cppcheck 2.19.0 all passed.
- [XP-targeted build run 43896578967](https://github.com/quilterrantdean/BiblioLED/actions/runs/43896578967): pinned w64devkit x86 2.10.0 build, all 8 XP-targeted tests (including `application_paths_tests`, diagnostic-audit checks, explicit ROLLBACK-failure recovery and abrupt-process-recovery coverage), PE32/i386, Windows GUI subsystem 5.01 and post-XP API/import checks passed. Artefact [14239447095](https://github.com/quilterrantdean/BiblioLED/actions/runs/43896578967/artifacts/14239447095), `BiblioLED-XP-PoC`, 1,554,248 bytes; ZIP SHA-256 `fbfbad00d193e85449700ba9e8d1044530eac3c22344de4c1dafad4eb18735e7`.



The current SQL hardening also releases any partially prepared SQLite statement on preparation failure, validates the full legacy `loans` declaration (INTEGER primary key, three TEXT NOT NULL columns, no unsupported extras), and refuses to create an audit event when the local timestamp cannot be obtained. Four isolated malformed-schema tests prove that a text ID, a missing primary key, a nullable required field, and an unsupported extra column are rejected without modifying the existing row. The version-1 `audit_events` table is also checked for all 11 expected columns, declared types, required/optional fields and INTEGER primary key; three negative cases (wrong action type, nullable required details, extra column) prove malformed v1 schemas are refused without changing the existing loan row. Backup compression and decompression flush the C runtime stream and then the operating-system file handle (`FlushFileBuffers` on Windows, `fsync` on POSIX) before the output is validated or installed. The XP artefact contains four files only, no database, and its test guide is UTF-8 with BOM. POSIX parent-directory metadata is not separately fsynced after rename, so the new step guarantees the file-data flush, not full final-name persistence through a sudden power cut.

The current SQL hardening also releases any partially prepared SQLite statement on preparation failure, validates the full legacy `loans` declaration (INTEGER primary key, three TEXT NOT NULL columns, no unsupported extras), and refuses to create an audit event when the local timestamp cannot be obtained. Four isolated malformed-schema tests prove that a text ID, a missing primary key, a nullable required field, and an unsupported extra column are rejected without modifying the existing row. The version-1 `audit_events` table is also checked for all 11 expected columns, declared types, required/optional fields and INTEGER primary key; three negative cases (wrong action type, nullable required details, extra column) prove malformed v1 schemas are refused without changing the existing loan row. Backup compression and decompression flush the C runtime stream and then the operating-system file handle (`FlushFileBuffers` on Windows, `fsync` on POSIX) before the output is validated or installed. The XP artefact contains four files only, no database, and its test guide is UTF-8 with BOM. POSIX parent-directory metadata is not separately fsynced after rename, so the new step guarantees the file-data flush, not full final-name persistence through a sudden power cut.

The UI implementation includes the top-level **Opções** menu, explicit audit-log retry command, event-driven refresh without polling, removal of redundant refresh UI, Ctrl+N/Ctrl+H accelerators, a resizable/reorderable/context-menu history window, statistics from a per-open snapshot, and UTF-8 resource/source handling. History controls are laid out in dialog units so they remain aligned when the window is resized.

On 2026-11-14, Actions history cleanup removed 223 older workflow runs and preserved the then-latest successful Core and XP-targeted runs (225 total before, 2 afterwards). Later pushes created new runs normally. `main` remains the unchanged Python reference; all implementation and documentation commits remain on `development`.

The user reconfirmed on 2026-11-16 that the executable runs well on Windows XP. The 2026-11-15 confirmation also covered the correction of secondary-window Portuguese text encoding. The exact tested artefact/hash, XP build and exercised scenarios have not been recorded, so this is a user-confirmed runtime smoke check rather than a reproducible full validation record. CI PE/build checks are not a substitute for broader target-machine testing.

The application remains a proof of concept, not the complete replacement. Guided startup recovery is implemented and covered by isolated tests, including Windows failure injection that proves rollback when a sidecar cannot be moved. Recent hardening also preserves invalid automatic-backup sidecars as a group, prevents reuse of temporary files with orphaned SQLite sidecars, uses a central executable-relative path model (including drive/UNC handling and no current-working-directory fallback), Unicode-aware Win32 filesystem access for database existence and INI settings, avoids daily backups for update/delete requests targeting nonexistent IDs, validates required directories, and fails closed if single-instance coordination cannot be established. The external audit log is scanned in fixed-size chunks rather than loading complete archived files into memory; its fingerprint de-duplication set still scales with the number of distinct historical events. Power-loss/interruption coverage, broader audit coverage and final target-system verification remain. The XP-targeted workflow builds with pinned w64devkit x86, runs both SQLite-store and external-log tests, checks PE32/i386, GUI subsystem 5.01 and imports, and publishes a disposable test artefact. The user's real database has not been opened, copied, migrated, or modified.

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
- [x] Build and run the Win32 application on Windows XP SP3 (user-confirmed working executable; exact artefact and test record still need to be captured).
- [ ] Measure startup time, executable size, memory use, and responsiveness.
- [ ] Decide whether Free Pascal still merits a direct comparison after the C++ XP test.

### Phase 3 — Foundation

- [x] Establish the initial project structure and CMake build configuration.
- [x] Implement the portable path model. A pure C++11 resolver derives every portable directory/file from the absolute executable path, validates drive/UNC paths, preserves Unicode, and fails closed instead of falling back to the process working directory.
- [x] Implement INI settings loading, validation, and persistence for the current prototype.
- [x] Implement SQLite schema versioning (PRAGMA user_version): new databases use version 1; the existing loans-only schema is recognised as version 0.
- [x] Implement the explicit, backed-up, transactional version 0 to 1 migration. It adds the internal audit table and indexes without rewriting loan rows.
- [x] Implement one automatic SQLite Online Backup snapshot before the first loan-data mutation per local calendar day; audit-only events and settings changes do not trigger it. Reuse a valid same-day snapshot and fail closed if a new snapshot cannot be created.
- [x] Implement compressed manual backups and a confirmed restore workflow with a separately classified, verified pre-restore safety copy; keep legacy uncompressed `.sqlite` backups restorable.
- [x] Detect a missing database after initialisation and refuse silent recreation.
- [x] Reject update/delete requests for missing record IDs before the automatic-backup gate, so failed no-op requests do not create daily backups.
- [x] Implement guided missing/corrupt-database recovery and reporting.
  - At startup, a missing previously initialised database or a database that cannot be opened offers an explicit backup-selection and confirmation flow. The selected backup is decompressed and schema/integrity checked, copied into a staging database, checked again, and installed only after the existing database and SQLite sidecars have been preserved under a recovery filename.
  - A failed candidate validation leaves the expected database and sidecars untouched. Staging names are also checked for stale `-journal`, `-wal`, and `-shm` files left by interrupted earlier attempts. Recovery is never automatic; the selected backup is retained, and the application reopens the installed database before continuing.
- [x] Store internal audit events in the same SQLite transaction as each loan create/edit/delete; include before/after values and migration events.
- [x] Synchronise internal audit events to the external append-only UTF-8 log using stable event fingerprints; recover an incomplete final line and rotate archives at the configured size.
- [x] When file logging is disabled, retain internal audit events in SQLite and queue external synchronisation; when re-enabled, synchronise the backlog. If enabled logging fails, allow reads but block further database mutations until recovery; retry through **Ficheiro > Tentar sincronizar registo de auditoria...**.
- [x] Record application startup/shutdown and successful manual backup/restore operations as internal audit events, using the existing transactional persistence and external-log synchronisation path.
- [x] Record general/backup settings changes and CSV exports in the transactional audit log.
- [x] Extend application audit-event coverage to warnings, errors, and failures. The main Win32 shell queues validation warnings and operation failures; history/statistics windows queue read and calculation failures. Diagnostic severities map to `application_warning`, `application_error`, and `application_failure`. These audit-only rows can be committed to SQLite while external file logging is unhealthy, but they do not clear its health flag or unblock loan mutations. Tests cover all three severities, invalid severity rejection and the unchanged mutation gate.
- [x] Harden external audit-log transport and verify read-only active-log recovery, incomplete archived-log rejection, rotation/idempotence and 64 KiB parser-boundary handling on both modern Windows and the XP-targeted build.
- [x] Separate audit-only writes (including startup/shutdown and settings changes) from loan-data backup triggers.
- [x] Implement the approved automatic-backup retention policy: 7 daily, 4 weekly, and 12 monthly snapshots. Only recognised automatic backups are pruned; manual and restore/migration safety backups are preserved. Unknown filenames are preserved.
- [x] Integrate statically linked zlib 1.4 for streaming gzip and zlib wrappers. Verify compressed round-trips and SQLite integrity; keep `.sqlite` backups compatible.
- [x] Add backup settings for format (`gzip` or `zlib`) and compression level (0–9, default 6) to `config/settings.ini` and a dedicated Win32 dialog. Changes are applied immediately to the open `LoanStore`, without restarting; a valid same-day automatic snapshot remains reused.
- [x] Use native filesystem path separators for generated backup paths (`\\` on Windows, `/` on POSIX), avoiding mixed separators in messages and file operations.
- [x] Treat Windows filesystem paths passed into the store as UTF-8, using Unicode Win32 file-attribute APIs for existence checks; test a database and compressed automatic backup under non-ASCII directory names.
- [x] Read and write `config/settings.ini` with wide-character Win32 file APIs, retaining the temporary-file, flush, and atomic-replacement write path.

The current CI validation baseline and XP artefact are recorded in **Current status** above. Keep that section authoritative when newer successful runs replace this baseline.

### Phase 4 — Application functionality and interface

- [x] Extract date parsing, calendar-day expiry, operational visibility, quota windows and remaining-quota calculation into portable core code; test these rules without the GUI.
- [x] Implement the main active-loans overview.
- [x] Implement the separate searchable/filterable history window; show all records including expired/future entries, filter by title/type/date/ID, and support edit/delete.
- [x] Implement revised statistics: annual evolution/comparison, daily activity, weekday/hour patterns, and mean/median intervals between distinct active dates; no progress or streak tracking.
- [x] Implement CSV export of the complete loan history, with UTF-8 BOM, semicolon delimiter, legacy-compatible columns, atomic replacement and an audit event.
- [x] Implement the general settings UI for limits, loan duration, diagnostics and log rotation; changes apply on next launch as specified by D-016.
- [x] Implement the About dialog with the embedded icon, version, build timestamp and XP target.
- [x] Add a dedicated backup-settings dialog for compression format and level, persisted in `config/settings.ini`.
- [x] Embed the existing application icon as a Win32 resource in `BiblioLED.exe`; do not ship or require a separate `.ico` file.
- [x] Make the main table columns reorderable by dragging their headers.
- [x] Persist column display order and individual widths in `config/settings.ini`; invalid settings files are preserved and are never overwritten by layout saving.

### UI refinement implementation

- [x] Added the top-level **Opções** menu; general and backup settings have been moved out of **Ficheiro**.
- [x] Added an explicit **Ficheiro > Tentar sincronizar registo de auditoria...** recovery command. Removed F5 as a main-window refresh/retry shortcut without losing the audit-log recovery path.
- [x] Removed the main-window refresh button and menu command, and removed the "Actualizado às ..." timestamp. The main list remains event-driven: startup, successful CRUD operations and successful database restore. No periodic polling was added.
- [x] Added a Win32 accelerator table and message-loop translation for Ctrl+N and Ctrl+H. Existing list-view activation handles Enter; Delete is handled from the list-view key notification.
- [x] Made the history dialog resizable, added a minimum tracking size, enabled header drag-and-drop and added a right-click Edit/Delete context menu. Double-click editing remains.
- [x] Removed the statistics dialog refresh button and F5 reload path. Statistics continue to load once when opened and recalculate from the cached snapshot when the selected year changes.
- [x] Added UTF-8 code-page declaration to the resource script and explicit UTF-8 source handling for MSVC. Corrected one statistics error-message conversion that widened UTF-8 bytes incorrectly; the user subsequently confirmed that secondary-window text encoding is now correct on XP.
- [x] Added click-to-sort to the main loans list and history list: alphabetical text sorting, numeric ID/days sorting, chronological date sorting, and ascending/descending toggling on repeated clicks.

The UI refinements, strict schema validation, SQL/transaction hardening, backup durability work, column sorting, central executable-relative path model, diagnostic audit coverage, explicit ROLLBACK-failure handling and abrupt-process recovery test passed Core and XP-targeted CI at source head `738c1d07a1a8256a80a78c5cc03135576c3c4aba` (runs 43896578995 and 43896578967); the XP matrix passed 8/8 tests. The user confirmed on 2026-11-16 that the executable runs well on XP. The resized history layout and recovery dialog have not been separately documented in the target-system test record; CI compilation and isolated tests do not prove those specific workflows.

### Phase 5 — Verification and delivery

- [x] Require the legacy `loans` table to have the exact supported four-column contract (INTEGER primary key; three TEXT NOT NULL fields); reject same-name but incompatible shapes without modifying their records. Add isolated tests for a text ID, missing primary key, and unsupported extra column.
- [x] Require the schema-version 1 `audit_events` table to match its complete 11-column type/nullability/primary-key contract; reject malformed action types, nullable required details, and extra columns without altering loan records.
- [ ] Continue expanding automated coverage for hardware power-loss, later schema migrations, settings persistence and UI workflows. The store suite now also forces a DDL failure after the audit table is created, then verifies the migration transaction removes that partial schema, retains the legacy record/version, and permits a clean reopen. The explicit `ROLLBACK` statement failure is now tested by denying the SQLite transaction action through a connection-local authorizer. The test verifies that `LoanStore` reports the failure and closes its connection, then reopens the isolated database and confirms that the uncommitted row is absent and `quick_check` succeeds. An additional isolated test starts a child process with an SQLite DELETE-journal transaction open, uses `synchronous=FULL`, a small cache and `sqlite3_db_cacheflush` to leave a hot rollback journal, then terminates the child with `TerminateProcess` (Windows) or `_exit` (POSIX). The parent confirms the journal exists, reopens the database, verifies that only the previously committed record remains, confirms that SQLite removes the journal, and passes `quick_check`. This simulates abrupt process death, not physical power loss or loss of storage-device write caches. Existing Windows recovery failure injection still verifies rollback after an original-sidecar move fails. All tests use synthetic/isolated data.
- [x] Add Cppcheck 2.19.0 warning, performance and portability analysis for the portable C++11 code. Its first run identified two uninitialised `LoanStore` state members; both are now explicitly initialised, and the clean analysis run passes.
- [x] Pin GitHub Actions to reviewed immutable Node 24 action commits: `actions/checkout` v7.0.1 and `actions/upload-artifact` v7.0.2.
- [x] Verify dependency provenance and immutable hashes: SQLite 3.53.4 official amalgamation (SHA3-256), zlib 1.4 official source archive (SHA-256), and w64devkit x86 2.10.0 release archive (SHA-256). The Cppcheck CI step now rejects any analyser version other than 2.19.0. Re-review these pins when intentionally updating dependencies.
- [x] Configure the initial GitHub Actions core-test workflow for pushes and pull requests to `development`.
- [x] Add a separate w64devkit x86 XP-targeted build workflow and reproducible CMake preset.
- [x] Confirm the w64devkit XP-targeted workflow passes and publish a downloadable test build.
- [x] Run the full portable core/store/backup test suite on Linux and Windows, including Windows audit-log tests.
- [x] Validate streamed audit-log parsing across 64 KiB read boundaries and confirm the full eight-test Windows/XP suite passes.
- [x] Require complete SQLite schema-metadata and integrity-check reads, including successful statement finalisation; validate normal, legacy and unsupported-schema paths in the full CI suite.
- [x] Check SQLite parameter-binding and statement-finalisation results in CRUD, audit, lookup, list and quota-count paths; confirm Linux, modern Win32 and the XP-targeted build/tests pass.
- [x] Centralise transaction rollback, check its result, and close the database connection if a failed rollback leaves the transaction active; verify partial migration DDL is rolled back and can be reopened cleanly.
- [x] Finalise any partial SQLite statement after a failed prepare, reject empty audit timestamps, and flush completed backup streams to stable storage before publishing the final backup.
- [ ] Complete and record the Windows XP SP3 validation matrix, including recovery, backup/restore, resource rendering and failure scenarios. The user has confirmed that the executable runs well and the secondary-window encoding is corrected; a reproducible artefact/hash, XP build and scenario record are still missing.
- [ ] Produce a portable 32-bit ZIP distribution.
- [x] Document the current proof-of-concept build, CI tests, dependencies, recovery, backup and audit behaviour in `docs/building.md`.
- [ ] Define the normal portable release process after physical Windows XP validation.

## Remaining implementation questions

- Confirm w64devkit x86 as the final compiler/toolchain after the Windows XP SP3 runtime test.
- Progressive history-loading threshold for very large histories. Final menu placement and refresh behaviour are now implemented according to the confirmed decisions.

Resolve these during implementation unless they materially change stored data or the agreed user experience. The UI refinements above are already user-approved; do not ask the user to repeat those decisions. Preserve the audit-log recovery capability when changing the F5 interaction.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
