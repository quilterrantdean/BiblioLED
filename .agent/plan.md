# Project plan

## Current status

**Stage:** the C++11/Win32/SQLite proof of concept now includes the agreed Win32 UI refinement pass and guided missing/corrupt-database recovery. Source commit `6773bd9627e6e86ea97226491aa13b2db4817bef` on `development` passed both CI workflows:

- [Core tests run 43655726935](https://github.com/quilterrantdean/BiblioLED/actions/runs/43655726935): Ubuntu core tests and modern Windows SDK Win32 build/store tests passed, including the new offline-recovery cases.
- [XP-targeted build run 43655726939](https://github.com/quilterrantdean/BiblioLED/actions/runs/43655726939): pinned w64devkit x86 build, all 7 XP-targeted CTest tests, PE32/i386 + GUI subsystem 5.01 + import inspection and artefact publication passed. Artefact [14138355192](https://github.com/quilterrantdean/BiblioLED/actions/runs/43655726939), `BiblioLED-XP-PoC` (1,523,056 bytes), archive SHA-256 `2bb59c1c72ef69992c73c36ff04516d7c47c48864e00a564715ae1b5abd3f8db`.

The UI implementation includes the top-level **Opções** menu, explicit audit-log retry command, event-driven refresh without polling, removal of redundant refresh UI, Ctrl+N/Ctrl+H accelerators, a resizable/reorderable/context-menu history window, statistics from a per-open snapshot, and UTF-8 resource/source handling. History controls are laid out in dialog units so they remain aligned when the window is resized.

On 2026-11-14, Actions history cleanup removed 223 older workflow runs and preserved the then-latest successful Core and XP-targeted runs (225 total before, 2 afterwards). Later pushes created new runs normally. `main` remains the unchanged Python reference; all implementation and documentation commits remain on `development`.

The user reports having tested the application manually. The exact tested artefact, Windows build and exercised scenarios have not yet been recorded; do not treat this as formal target-runtime sign-off. CI PE/build checks are not a substitute for testing on the target machine.

The application remains a proof of concept, not the complete replacement. Guided startup recovery is now implemented and covered by isolated store tests, but interrupted-install failure injection, broader audit coverage and final target-system verification remain. The XP-targeted workflow builds with pinned w64devkit x86, runs both SQLite-store and external-log tests, checks PE32/i386, GUI subsystem 5.01 and imports, and publishes a disposable test artefact. The user's real database has not been opened, copied, migrated, or modified.

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
- [x] Implement guided missing/corrupt-database recovery and reporting.
  - At startup, a missing previously initialised database or a database that cannot be opened offers an explicit backup-selection and confirmation flow. The selected backup is decompressed and schema/integrity checked, copied into a staging database, checked again, and installed only after the existing database and SQLite sidecars have been preserved under a recovery filename.
  - A failed candidate validation leaves the expected database and sidecars untouched. Recovery is never automatic; the selected backup is retained, and the application reopens the installed database before continuing.
- [x] Store internal audit events in the same SQLite transaction as each loan create/edit/delete; include before/after values and migration events.
- [x] Synchronise internal audit events to the external append-only UTF-8 log using stable event fingerprints; recover an incomplete final line and rotate archives at the configured size.
- [x] When file logging is disabled, retain internal audit events in SQLite and queue external synchronisation; when re-enabled, synchronise the backlog. If enabled logging fails, allow reads but block further database mutations until recovery; retry through **Ficheiro > Tentar sincronizar registo de auditoria...**.
- [x] Record application startup/shutdown and successful manual backup/restore operations as internal audit events, using the existing transactional persistence and external-log synchronisation path.
- [x] Record general/backup settings changes and CSV exports in the transactional audit log.
- [ ] Extend audit coverage to warnings, errors, and failures.
- [x] Separate audit-only writes (including startup/shutdown and settings changes) from loan-data backup triggers.
- [x] Implement the approved automatic-backup retention policy: 7 daily, 4 weekly, and 12 monthly snapshots. Only recognised automatic backups are pruned; manual and restore/migration safety backups are preserved. Unknown filenames are preserved.
- [x] Integrate statically linked zlib 1.4 for streaming gzip and zlib wrappers. Verify compressed round-trips and SQLite integrity; keep `.sqlite` backups compatible.
- [x] Add backup settings for format (`gzip` or `zlib`) and compression level (0–9, default 6) to `config/settings.ini` and a dedicated Win32 dialog. Changes are applied immediately to the open `LoanStore`, without restarting; a valid same-day automatic snapshot remains reused.
- [x] Use native filesystem path separators for generated backup paths (`\\` on Windows, `/` on POSIX), avoiding mixed separators in messages and file operations.

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
- [x] Added UTF-8 code-page declaration to the resource script and explicit UTF-8 source handling for MSVC. Corrected one statistics error-message conversion that widened UTF-8 bytes incorrectly.

The UI refinement changes above passed Core and XP-targeted CI at `bda018b0ee5924c736b9388cf4a6efa45dc1b`. The subsequent guided-recovery implementation passed Core and XP-targeted CI at `6773bd9627e6e86ea97226491aa13b2db4817bef` (runs 43655726935 and 43655726939). The UTF-8 resource encoding fix, resized history layout and recovery dialog still require visual/behavioural confirmation on the user's target system; CI compilation and isolated tests cannot prove physical XP runtime behaviour.

### Phase 5 — Verification and delivery

- [ ] Continue expanding automated coverage for interrupted recovery/install failures, rollback, migration, settings persistence and UI workflows. The store suite now covers recovery from a missing database, preservation of a corrupt database and SQLite sidecars, rejection of an invalid candidate without touching the destination, and reopening the recovered database. All tests use synthetic/isolated data.
- [ ] Add static analysis and dependency checks.
- [x] Configure the initial GitHub Actions core-test workflow for pushes and pull requests to `development`.
- [x] Add a separate w64devkit x86 XP-targeted build workflow and reproducible CMake preset.
- [x] Confirm the w64devkit XP-targeted workflow passes and publish a downloadable test build.
- [x] Run the full portable core/store/backup test suite on Linux and Windows, including Windows audit-log tests.
- [ ] Validate the application and tests on Windows XP as well as newer systems.
- [ ] Produce a portable 32-bit ZIP distribution.
- [ ] Document the build and release process.

## Remaining implementation questions

- Confirm w64devkit x86 as the final compiler/toolchain after the Windows XP SP3 runtime test.
- Progressive history-loading threshold for very large histories. Final menu placement and refresh behaviour are now implemented according to the confirmed decisions.

Resolve these during implementation unless they materially change stored data or the agreed user experience. The UI refinements above are already user-approved; do not ask the user to repeat those decisions. Preserve the audit-log recovery capability when changing the F5 interaction.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
