# BiblioLED

Personal application for recording and managing the user's own BiblioLED loans. This is not public-library management software.

## Project status

The `development` branch contains an early C++ / Win32 / SQLite proof of concept. The legacy Python/Tkinter implementation remains in the repository as a functional reference while the replacement is developed.

The new prototype is not yet validated on Windows XP and must not be used with the user's real database.

## Current direction

- Windows XP SP3, 32-bit (Win32) is the primary target.
- Native Win32 controls and a compact, utility-style interface.
- SQLite is the required database engine.
- C++11 and CMake are used for the initial proof of concept.
- SQLite 3.53.4 is pinned for the XP prototype because SQLite 3.54.0 introduced a Windows SRW-lock dependency unavailable on XP.
- User data, settings, backups, exports, and logs will remain in the portable application folder.

## Build and tests

See [docs/building.md](docs/building.md).

GitHub Actions currently builds and tests the SQLite store on Linux and builds the 32-bit Win32 application plus runs the store tests on a modern Windows runner. Both jobs pass. This is not Windows XP runtime validation: an XP-compatible compiler/toolchain and a real XP SP3 test remain outstanding.

## Development notes

- `AGENTS.md` contains instructions for coding agents.
- `.agent/plan.md` tracks the project phases.
- `.agent/decisions.md` records user-approved requirements.
- `.agent/architecture.md` describes the target component boundaries.
- `.agent/codebase-review.md` records the legacy implementation review and prototype status.

All assistant development work takes place on `development`. Never commit directly to `main` or `master`.
