# BiblioLED

Personal application for recording and managing the user's own BiblioLED loans. This is not public-library management software.

## Project status

The `development` branch contains an early C++ / Win32 / SQLite proof of concept. The legacy Python/Tkinter implementation remains in the repository as a functional reference while the replacement is developed.

The new prototype has passed XP-targeted build checks but has not yet been run on Windows XP itself. It includes automatic per-session backups, manual backups, confirmed restore with a pre-restore safety copy, protection against silently recreating a missing initialized database, schema version 1, an explicit backed-up migration from the legacy version 0 schema, transactional internal auditing, and a recoverable rotating UTF-8 audit log covering loan changes, migration, application startup/shutdown, and successful manual backup/restore operations. The application icon is embedded in the executable; main-table columns are draggable and their order and widths persist in `config/settings.ini`. The current uncompressed, per-session automatic backup policy is under review because the startup audit event currently triggers a snapshot on every normal launch; compression and bounded retention for automatic backups are being evaluated. Guided recovery, later migrations, the full settings UI, and the complete application remain outstanding. It is still a test build and must not be used with the user's real database.

## Current direction

- Windows XP SP3, 32-bit (Win32) is the primary target.
- Native Win32 controls and a compact, utility-style interface.
- SQLite is the required database engine.
- C++11 and CMake are used for the initial proof of concept.
- The initial XP-targeted compiler baseline is the x86 edition of w64devkit; the dedicated workflow pins a release and checks the resulting PE target.
- SQLite 3.53.4 is pinned for the XP prototype because SQLite 3.54.0 introduced a Windows SRW-lock dependency unavailable on XP.
- SQLite user_version=1 identifies the current schema. Legacy version 0 databases require explicit, backed-up migration before writes.
- The application icon is compiled into `BiblioLED.exe`; no separate `.ico` is required in the runtime folder or ZIP.
- Main-table column order and widths are saved to `config/settings.ini`.
- User data, settings, backups, exports, and logs remain in the portable application folder.

## Build and tests

See [docs/building.md](docs/building.md).

GitHub Actions builds and tests the SQLite store on Linux, checks a modern Win32 build, and builds with the XP-targeted w64devkit x86 toolchain. The XP build passes the loan-store and audit-log tests and PE32/i386/subsystem checks, then publishes a disposable test artefact. No workflow running on a modern Windows host can replace the final execution test on Windows XP SP3.

## Development notes

- `AGENTS.md` contains instructions for coding agents.
- `.agent/plan.md` tracks the project phases.
- `.agent/decisions.md` records user-approved requirements.
- `.agent/architecture.md` describes the target component boundaries.
- `.agent/codebase-review.md` records the legacy implementation review and prototype status.

All assistant development work takes place on `development`. Never commit directly to `main` or `master`.
