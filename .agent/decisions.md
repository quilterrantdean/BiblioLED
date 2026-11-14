# Project decisions

This file records decisions explicitly agreed with the user. Status labels distinguish confirmed requirements from provisional technical direction.

## Product scope and user experience

| ID | Decision | Status |
|---|---|---|
| D-001 | BiblioLED is a personal tool to record and manage the user's own loans. It is not public-library management software and does not need a library catalogue. | Confirmed |
| D-002 | The existing core purpose and useful functionality should be retained while the interface and internal architecture are refined. | Confirmed |
| D-003 | The main window is an operational overview showing active loans and useful additional information. The full, potentially long history stays in a separate window. | Confirmed |
| D-004 | Visual direction: compact, clear, functional Windows utility, inspired by NirSoft and skeeto/nullprogram tools. Prioritise legibility and information hierarchy; avoid decorative cards and visual effects. | Confirmed |
| D-005 | The history window supports real-time filtering. Filtering should be responsive and should avoid unnecessary repeated work for large histories. Exact implementation parameters remain open. | Confirmed |
| D-006 | Remove the reading-progress/record-streak statistics tab. Keep two areas: general/evolution (totals, type distribution, annual evolution and year comparison) and usage patterns (activity calendar, weekday/hour distribution, and mean/median interval between loans). | Confirmed |
| D-007 | The About dialog should show the existing application icon, application name, version, a short description, and build date. Do not add copyright, licence, or website information. Version and build date are distinct values. | Confirmed |
| D-008 | Preserve the existing repository icon (`port.ico`). | Confirmed |
| D-009 | User-facing text is European Portuguese, using pre-1990 spelling conventions. | Confirmed |
| D-010 | Keyboard navigation and shortcuts are first-class. The main window is resizable and should work from 800 × 600 pixels. | Confirmed |
| D-011 | Follow the active Windows visual theme and use native Win32 controls where practical. | Confirmed |
| D-053 | Future-dated loan records are shown in the main loan list in the same way as other records and immediately reserve capacity against the daily and monthly limits when entered. Their expiry is calculated from the loan date recorded for them. | Confirmed |
| D-054 | The main table is labelled simply “Empréstimos”. It includes future-dated loans, while loans whose expiry date has passed are omitted from the main view and remain available in the full history. | Confirmed |
| D-055 | The application icon must be embedded as a native Win32 resource in `BiblioLED.exe`. The executable must not require a separate `.ico` file at runtime or in the portable distribution. Keep the source icon in the repository for resource compilation. | Confirmed |
| D-056 | The main loan table's columns must be user-reorderable by dragging the headers. The display order and individual column widths must persist between launches, stored in the existing portable `config/settings.ini` file. Invalid settings files must remain preserved; UI preferences must not overwrite an invalid settings file. | Confirmed |

## Data, settings, and portability

| ID | Decision | Status |
|---|---|---|
| D-012 | SQLite is the required database technology. The user's existing SQLite database is the starting point. | Confirmed |
| D-013 | The current SQLite schema must remain readable/migratable. The current table is `loans(id, title, loan_date, type)`. | Confirmed baseline |
| D-014 | The old CSV-to-SQLite import path is vestigial and may be removed. CSV remains a user-facing record-export format; exporting records is not a database backup. | Confirmed |
| D-015 | Current intended defaults: daily limit 5, monthly limit 20, rolling monthly window 30 days, loan duration 21 days. All are editable in the application and stored outside the executable. | Confirmed |
| D-016 | Settings are managed in the application. Changes are saved but only take effect on the next application launch. Operational values must not be hardcoded into the executable. | Confirmed |
| D-017 | The application is fully portable: executable, settings, database, backups, exports, logs, and resources are contained in one organised application folder. No AppData dependency, registry installation, service, or installer is required. | Confirmed |
| D-018 | Missing settings are created with defaults. Invalid settings are preserved; the application warns and uses defaults in memory. When the user chooses to recreate an invalid file, preserve the original with an `.invalid` suffix. | Confirmed |
| D-019 | Current prototype: database backups are uncompressed SQLite files. Automatic backups are created immediately before the first database modification in each session; no backup is created for a read-only session. | Under review |
| D-020 | Current prototype: retain all backups indefinitely. No automatic retention or deletion policy; the user manages cleanup manually. | Under review |
| D-021 | A manual full-database backup is a separate operation from exporting selected/all records to CSV or another record format. | Confirmed |
| D-022 | Before restoring a backup, create and validate a backup of the current database, even if the current session has not modified it. | Confirmed |
| D-023 | Validate every backup immediately after creation using SQLite integrity checking. Validate a candidate backup's integrity and schema compatibility before restore. | Confirmed |
| D-024 | Backups are stored in a `backups` directory alongside the database, with timestamp-based names. No compression. | Confirmed |
| D-025 | Database corruption or inability to open the expected database must not silently create a new empty database. Enter a protective state, preserve the file, explain the problem, and offer recovery/restore. | Confirmed |
| D-026 | Run a quick SQLite integrity check at normal startup. Run a full integrity check when corruption is suspected, after an unexpected shutdown, and after backup creation. | Confirmed |
| D-027 | Schema migrations require explicit user confirmation every time. Before migration, create and validate a backup; report source/target schema and changes; migrate transactionally; verify the result; retain the original backup. | Confirmed |
| D-028 | Support older database/backup schemas through explicit, versioned migrations. Never modify the source backup file during migration. | Confirmed |
| D-029 | Never silently discard invalid/inconsistent records during migration. Report them, preserve the source data, and place unconvertible records in a separate recovery area, excluded from normal calculations until corrected. | Confirmed |
| D-030 | If an automatic backup fails, block database modifications for that session; read-only access may continue. | Confirmed |

## Diagnostics and audit

| ID | Decision | Status |
|---|---|---|
| D-031 | Diagnostic logs are enabled by default and can be disabled in application settings. Logs remain local; there is no remote telemetry. | Confirmed |
| D-032 | Use one active append-only text log. When it exceeds a configurable size (default 5 MB), archive it and start a new active log. Preserve all archives; never delete automatically. | Confirmed |
| D-033 | Logs are plain UTF-8 text, with timestamp, event, affected record ID where relevant, context, and outcome. | Confirmed |
| D-034 | Record relevant application activity comprehensively: startup/shutdown, loan creation/edit/deletion, before/after values, settings changes, database operations, migrations, backups/restores, exports, warnings, errors, and failures. | Confirmed |
| D-035 | No in-app log viewer or Explorer shortcut is required. The user will inspect files directly in the logs directory. | Confirmed |
| D-036 | If logging is enabled and writing the audit log fails, block database mutations until logging is available again; allow read-only use and explain the failure. | Confirmed |
| D-037 | Keep database changes and audit events consistent. Store audit events transactionally in SQLite and synchronise them to the external append-only UTF-8 text log. Use stable event fingerprints to avoid duplicates and recover an incomplete final line after interruption. Rotate the active log at the configured size (default 5 MB), retaining all archives. When external logging is deliberately disabled, keep internal SQLite auditing and queue the external backlog; resume synchronisation when logging is re-enabled. When logging is enabled but the external log cannot be synchronised, allow reads but block further database mutations until it recovers. | Confirmed |

## Architecture, quality, and delivery

| ID | Decision | Status |
|---|---|---|
| D-038 | Separate the application into interface, business rules, persistence, and infrastructure responsibilities. | Confirmed |
| D-039 | The GUI must not access SQLite directly or contain business rules. Business rules must be runnable/testable without starting the GUI. | Confirmed |
| D-040 | Use a single application instance per portable installation. A second launch should activate the existing instance. Separate copies in different folders may run independently. | Confirmed |
| D-041 | Use centralised error handling with recoverable, operational, and critical error categories. | Confirmed |
| D-042 | Tests must be exhaustive and must never touch the user's real database or settings. Use isolated temporary databases/configurations and synthetic data. | Confirmed |
| D-043 | Test business rules, persistence, migrations, backups/restores, settings, logging, and GUI workflows. Tests should be runnable on Windows XP and newer systems where applicable. | Confirmed |
| D-044 | GitHub Actions should run tests and static checks for development changes and pull requests. Failing tests/critical checks block approval; do not bypass checks. | Confirmed |
| D-045 | Build tooling and dependencies may be updated. Record versions and build parameters so a particular previous build can be reproduced; do not freeze tools indefinitely. | Confirmed |
| D-046 | Prefer minimal dependencies and native facilities where appropriate. Evaluate dependencies for XP compatibility, performance, memory use, and maintenance cost. | Confirmed |
| D-047 | Maintain concise technical documentation for architecture, database/schema migrations, build, dependencies, and tests. | Confirmed |
| D-048 | Primary platform is Windows XP SP3 32-bit. The deliverable is a Win32 executable, portable ZIP, normal-user privileges, no installer or services. | Confirmed |
| D-049 | The interface should use native Win32 controls where technically practical and follow the active Windows theme. Minimum target resolution is 800 × 600. | Confirmed |
| D-050 | Long operations should keep the UI responsive, report progress where useful, prevent incompatible simultaneous operations, and avoid unsafe interruption. | Confirmed |
| D-051 | Application updates are manual. Replacing program files must preserve database, settings, backups, exports, and logs. | Confirmed |
| D-052 | Never commit directly to `main` or `master`. Assistant development work is only on `development`. PRs/Auto-merge are permitted after required checks; no manual merge or direct update of primary branches without a specific user request. | Confirmed |

## Provisional technology direction

- **Preferred candidate:** C++ with native Win32 and SQLite.
- The initial proof of concept uses C++11, CMake, and SQLite 3.53.4. The compiler/toolchain is not yet final.
- **SQLite XP baseline:** 3.53.4 is pinned because 3.54.0 introduced a Windows dependency on SRW (Slim Reader/Writer) locks unavailable on XP. Reconsider later releases only after actual XP validation. See https://sqlite.org/forum/info/2f3abc1a9d346e4a779003d2e21204255bd7462c99396ffeee46025e22fe0f48.
- This version pin is a compatibility baseline, not a permanent prohibition on upgrades. Re-evaluate later SQLite releases only after testing the Windows XP target.
- Build and run the proof of concept on Windows XP before finalising the language/toolchain decision.
- Free Pascal may be included as a comparison candidate if the C++ proof of concept exposes a meaningful disadvantage.
