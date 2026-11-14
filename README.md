# BiblioLED

Personal application for recording and managing the user's own BiblioLED loans. This is not public-library management software.

## Project status

The `development` branch contains an early C++ / Win32 / SQLite proof of concept. The legacy Python/Tkinter implementation remains in the repository as a functional reference while the replacement is developed.

The new prototype has passed XP-targeted build checks but has not yet been run on Windows XP itself. It remains a test build and must not be used with the user's real database.

## Current direction

- Windows XP SP3, 32-bit (Win32) is the primary target.
- Native Win32 controls and a compact, utility-style interface.
- SQLite is the required database engine.
- C++11 and CMake are used for the initial proof of concept.
- The initial XP-targeted compiler baseline is the x86 edition of w64devkit; the dedicated workflow pins a release and checks the resulting PE target.
- SQLite 3.53.4 is pinned for the XP prototype because SQLite 3.54.0 introduced a Windows SRW-lock dependency unavailable on XP.
- User data, settings, backups, exports, and logs will remain in the portable application folder.

## Build and tests

See [docs/building.md](docs/building.md).

GitHub Actions builds and tests the SQLite store on Linux, checks a modern Win32 build, and builds with the XP-targeted w64devkit x86 toolchain. The XP build passes its SQLite test and PE32/i386/subsystem checks, then publishes a disposable test artifact. No workflow running on a modern Windows host can replace the final execution test on Windows XP SP3.

## Development notes

- `AGENTS.md` contains instructions for coding agents.
- `.agent/plan.md` tracks the project phases.
- `.agent/decisions.md` records user-approved requirements.
- `.agent/architecture.md` describes the target component boundaries.
- `.agent/codebase-review.md` records the legacy implementation review and prototype status.

All assistant development work takes place on `development`. Never commit directly to `main` or `master`.
