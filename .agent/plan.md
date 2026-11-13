# Project plan

## Current status

**Stage:** requirements and architecture planning.

The initial requirements review has covered data integrity and recovery, portable file/configuration management, architecture and maintenance, and the preferred technology direction. No application code has been changed as part of this planning work.

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
- [ ] Finish the high-level UI specification for the main window, history, menus, settings, statistics, and About dialog.
- [ ] Decide unresolved behaviours that materially affect the data model or user experience.

### Phase 2 — Technology proof of concept

- [ ] Evaluate C++ with native Win32 controls and SQLite as the leading candidate.
- [ ] Keep other viable candidates open until measured, including Free Pascal if useful.
- [ ] Select a compiler/toolchain, C++ language baseline, build system, and SQLite integration method.
- [ ] Build a small 32-bit proof of concept with a window, table, and basic SQLite CRUD operations.
- [ ] Build and run the proof of concept on Windows XP SP3.
- [ ] Measure startup time, executable size, memory use, and responsiveness.
- [ ] Make the final technology decision from observed results, not assumptions.

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

- Exact definition and display rules for an "active" loan, including how future-dated records are treated.
- Final field/column arrangement and commands in the main window and history window.
- Exact real-time filter controls and the threshold, if any, for progressive history loading.
- Exact audit-log synchronisation behaviour when file logging is disabled.
- Final compiler, toolchain, build system, and language baseline.
- The final portable directory names and exact configuration-file format.

Do not implement speculative answers to these questions where they could change stored data or user-visible behaviour. For low-impact implementation details, choose a conventional, simple, and safe default.
