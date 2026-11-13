# Project plan

## Current status

**Stage:** first C++/Win32/SQLite proof of concept builds and passes Linux, modern Win32, and XP-targeted w64devkit CI; Windows XP runtime validation remains outstanding.

The initial requirements and architecture are documented. The first implementation slice contains a SQLite store, native Win32 main window with basic loan CRUD, settings file, isolated store tests, and CI for Linux, modern Win32, and the XP-targeted w64devkit x86 build. All three checks pass on commit 40df8aee. The XP-targeted build passes its SQLite test and PE32/i386/subsystem 5.01 checks. A disposable test artefact is published. The application has not yet been run on Windows XP. The user's real database has not been opened or modified.

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
- [ ] Implement SQLite access and schema versioning.
- [ ] Implement safe migration from the current database schema, with explicit confirmation.
- [ ] Implement backup, validation, restore, and recovery flows.
- [ ] Implement diagnostic and audit logging.

### Phase 4 — Application functionality and interface

- [ ] Implement the business rules independently of the GUI.
- [ ] Implement the main active-loans overview.
- [ ] Implement the separate searchable/filterable history window.
- [ ] Implement revised statistics.
- [ ] Implement settings and the About dialog.
- [ ] Preserve the existing application icon from the repository.

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
- Final settings-file format and any schema-version marker.

Resolve these during implementation unless they materially change stored data or the agreed user experience.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
