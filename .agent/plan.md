# Project plan

## Current status

**Stage:** first C++/Win32/SQLite proof of concept underway.

The initial requirements and architecture are documented. The first implementation slice now contains a SQLite store, a native Win32 main window with basic loan CRUD, a settings file, isolated store tests, and a GitHub Actions core-test workflow. It is not yet validated on Windows XP. The user's real database has not been opened or modified.

The `development` branch was created from the current `main` branch. This initial change adds project/agent documentation only.

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
- [ ] Resolve only remaining questions that materially affect data or user-visible behaviour.

### Phase 2 — Technology proof of concept

- [x] Establish C++ / native Win32 / SQLite as the leading proof-of-concept direction.
- [x] Establish C++11 and CMake for the initial prototype.
- [x] Pin SQLite 3.53.4 for the XP compatibility baseline; newer releases require revalidation.
- [x] Add the first native Win32 window, list view, loan dialog, and SQLite CRUD implementation.
- [x] Add isolated SQLite store tests and a Linux GitHub Actions test workflow.
- [ ] Select and document the final XP-compatible compiler/toolchain.
- [ ] Build and run the Win32 application on Windows XP SP3.
- [ ] Measure startup time, executable size, memory use, and responsiveness.
- [ ] Decide whether Free Pascal still merits a direct comparison after the C++ XP test.

### Phase 3 — Foundation

- [ ] Establish the project structure and build configuration.
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
- [ ] Configure GitHub Actions for development branches and pull requests.
- [ ] Validate the application and tests on Windows XP as well as newer systems.
- [ ] Produce a portable 32-bit ZIP distribution.
- [ ] Document the build and release process.

## Open questions to resolve during design or implementation

- Final column arrangement and exact visual treatment of future-dated loans and expiry status in the main “Empréstimos” table (future-dated inclusion, immediate quota reservation, and exclusion of expired loans are already defined).
- Final field/column arrangement and commands in the main window and history window.
- Exact real-time filter controls and the threshold, if any, for progressive history loading.
- Exact audit-log synchronisation behaviour when file logging is disabled.
- Final XP-compatible compiler/toolchain; the prototype currently uses C++11 and CMake.
- The final portable directory names and exact configuration-file format.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
