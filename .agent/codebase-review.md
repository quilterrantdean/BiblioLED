# Existing codebase review

## Scope and baseline

The review was performed against the `main` branch at commit:

`1d5c5adb885356beed7d8d1adb14abc348e5f985`

The repository is a small Python/Tkinter application with SQLite persistence. The following files were inspected:

- `biblioled.pyw` — main window, navigation, history, statistics, and application flow.
- `data_utils.py` — SQLite operations, CSV import/export, calculations, and statistics.
- `ui_components.py` — dialogs and record-entry form.
- `gui_utils.py` — window and Treeview helpers, tooltips, keyboard navigation.
- `config.py` — defaults, paths, and JSON settings.
- `README.md` — requirements and usage.

The repository also contains `port.ico`, which must be retained as the application icon.

This is an initial static review, not a complete runtime test or exhaustive audit. Findings below are based on the inspected source.

## Current implementation map

- `biblioled.pyw` owns most of the application workflow and constructs the main, history, and statistics windows.
- `data_utils.py` provides direct SQLite access and mixes storage operations with business calculations.
- `config.py` holds global defaults and loads JSON settings.
- `ui_components.py` implements modal dialogs and the new/edit record form.
- `gui_utils.py` provides reusable Tkinter helpers.
- SQLite table baseline: `loans(id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, loan_date TEXT NOT NULL, type TEXT NOT NULL)`.

## Findings to address

### Data integrity and migration

1. **CSV import/export formats do not match.** The legacy CSV importer parses `LoanDate` using the date-only format (`YYYY-MM-DD`), while the CSV exporter writes the date and time using the full datetime format (`YYYY-MM-DD HH:MM`). Exported files are therefore not reliably re-importable by the current migration path.
2. **Legacy CSV migration skips invalid rows silently.** Unparseable rows are skipped. The CSV is then moved to a `.bak` file after the import routine completes, without a per-record recovery report. This is one reason the user has decided to retire CSV import and keep SQLite as the primary data store.
3. **History reads hide errors.** `read_history()` catches broad exceptions and returns an empty list. It also skips rows whose date strings cannot be parsed. A read failure or malformed records can consequently look like missing history.
4. **Database initialisation has no protective recovery workflow.** `init_db()` creates the table if absent and reports an error as text if initialisation fails. The application does not implement the agreed protective mode, backup validation, restore flow, or explicit schema migration workflow.

### Business rules and statistics

5. **Future-dated records need an explicit policy.** Dashboard queries for daily/monthly limits exclude records after the current time, while the expiry query has no upper time bound. The displayed treatment of future-dated records is therefore not fully consistent and should be specified before implementation.
6. **The interval statistic is mislabelled for the intended purpose.** The current `avg_days_between` calculation uses distinct calendar dates containing activity, rather than the interval between individual loan records. The revised statistics should calculate and label mean/median intervals consistently.
7. **Availability and expiry rules should share explicit date-boundary definitions.** The current code calculates monthly release dates from a loan timestamp plus the configured window. Boundary inclusivity and same-day behaviour should be covered by tests.

### Configuration and filesystem

8. **Paths depend on the process working directory.** Database, icon, settings, export, and legacy CSV paths are relative. Launching the application from a different working directory can make it use a different database or fail to find resources.
9. **Settings errors are swallowed.** `load_user_settings()` catches broad exceptions without reporting which setting or file failed. Settings persistence is not exposed through a dedicated settings window.
10. **The export directory is created as a side effect of importing configuration.** File-system setup should move into explicit application initialisation/infrastructure code.

### User interface and maintainability

11. **The main module has too many responsibilities.** `biblioled.pyw` contains main-window layout, history management, statistics rendering, event handling, and application workflow.
12. **History filtering rebuilds the entire Treeview.** There is already a 250 ms debounce and an in-memory history cache, but each search repopulates the table by deleting and reinserting every visible row. Keep the real-time behaviour while improving scalability only when needed.
13. **Statistics chart sizing is largely fixed.** The annual chart uses fixed canvas coordinates, so resizing and different data volumes may affect layout.
14. **Calendar tooltip ownership is not tied to each statistics window.** The application stores a single tooltip object on the main app instance and may reuse it when a new statistics window/canvas is created.
15. **The next-slots dialog assumes metrics are available.** If refreshing the dashboard fails and no valid status cache exists, the dialog may attempt to access fields on a missing cache.
16. **The About dialog is not aligned with the agreed design.** It currently reports Python version, SQLite, and an XP support statement. The new dialog should show the existing icon, product name, application version, short purpose description, and build date.

## Priority

1. Protect the existing SQLite database and implement the backup/migration/recovery requirements.
2. Define date and loan-status semantics before changing calculations.
3. Separate business rules and persistence from the UI.
4. Establish portable paths and robust settings/log handling.
5. Implement the revised Win32 user interface.
6. Add exhaustive automated tests and static analysis.

Do not treat this review as permission to start changing application code. It is a baseline for planning the rewrite/restructure on `development`.


## C++ / Win32 / SQLite replacement status

The legacy Python/Tkinter code remains unchanged as the behavioural reference. The new implementation is being developed alongside it.

### Implemented

- C++11 core, CMake build, native Win32 application; `port.ico` is compiled into the executable as a Win32 icon resource. No separate icon file is required at runtime or in the portable ZIP.
- SQLite 3.53.4 statically linked in Windows builds. This version is pinned for the XP target; SQLite 3.54.0 introduced a Windows SRW-lock dependency unavailable on XP.
- Current table compatibility: `loans(id, title, loan_date, type)`; prepared-statement CRUD and quota counts.
- Main Win32 window with loan list, create/edit/delete, delete confirmation, external INI limits, single-instance coordination, and application-relative storage paths. Startup fails closed if the named mutex cannot be created; it does not silently run without the per-installation single-instance guarantee.
- Future-dated loans appear in the main list and are counted immediately; expired loans remain in the database but are omitted from that view.
- Daily automatic SQLite Online Backup snapshot immediately before the first loan-data mutation of the local day. Same-day snapshots are reused; startup/shutdown and other audit-only writes do not trigger snapshots. New snapshots are integrity-checked and compressed with zlib 1.4 as gzip or zlib streams. A failed snapshot/compression/round-trip validation blocks the pending loan mutation.
- Manual compressed backups use a separate `BiblioLED_manual_*` namespace. Restore accepts `.sqlite.gz`, `.sqlite.zlib`, and legacy uncompressed `.sqlite` files. The selected source is fully decompressed and integrity/schema-checked before a separate compressed `BiblioLED_safety_*` snapshot is made. The restored database is checked again; the code attempts rollback from the safety copy if post-restore verification fails.
- Automatic backups use the `BiblioLED_auto_*` namespace and only those files are subject to retention: 7 daily, 4 weekly, and 12 monthly. Manual and safety backups are excluded; unrecognised filenames are preserved.
- A `config/database.initialized` marker distinguishes first use from a missing database. If the marker exists but `data/biblioteca.sqlite` is absent, startup offers guided recovery instead of silently creating an empty database. A failed open of an existing database offers the same explicit recovery path. Invalid automatic-backup files are moved together with their `-journal`, `-wal`, and `-shm` sidecars; a partial move is rolled back where possible.
- Schema versions use SQLite PRAGMA user_version: new databases are version 1; the existing loans-only schema is version 0 and is opened read-only until explicit confirmation.
- Version 0 to 1 migration creates and validates a full backup first, then transactionally creates the audit_events table and indexes, records the migration, and verifies the result. Existing loan rows are not rewritten or discarded; the original version-0 backup remains unchanged.
- Create/edit/delete operations insert internal audit events in the same SQLite transaction as the loan change. Events include action, timestamp, loan ID, before/after values, and details. If the audit insert fails, the associated loan operation rolls back. The migration event is also recorded.
- The store exposes a read API for internal audit events. `src/infrastructure/audit_log.cpp` synchronises them to `logs/audit.log` using stable event fingerprints; it avoids duplicates across active and archived files and recovers an incomplete final line in the active log.
- External audit logs rotate at the configured maximum size (default 5 MB), with timestamped archives retained indefinitely. If external logging is disabled, SQLite auditing continues and the file backlog is queued. If enabled logging fails, reads remain possible but the store blocks further mutations until synchronisation recovers; retry via **Ficheiro > Tentar sincronizar registo de auditoria...**. Current event coverage includes loan CRUD, schema migration, application startup/shutdown, manual backup/restore, general/backup settings changes and CSV export. Warnings, errors, and failure events remain outstanding.
- Store paths are UTF-8 on Windows. Existence checks use `GetFileAttributesW`, and settings INI reads/writes use wide-character Win32 file APIs; Windows tests create and reopen a database and compressed backup beneath a directory containing non-ASCII characters. Startup validates config/data/backups/exports/logs as directories, aborts before database access on file/directory collisions, and fails closed if the single-instance mutex cannot be created.
- The main table enables native header drag-and-drop. Logical column order and individual widths are loaded from and saved to `config/settings.ini`. Layout saving is disabled when the settings file was invalid, preserving the original file.
- Isolated tests cover CRUD, UTF-8, future-dated quota counts, daily snapshot reuse, audit-only writes without backup triggers, rejection/preservation of an unrelated but integrity-valid SQLite file under the automatic-backup namespace, compressed automatic/manual/safety backup round-trips, GZIP/zlib at levels 0/1/6/9, truncated/trailing stream rejection, retention with manual/safety and malformed-name preservation, compressed and legacy `.sqlite` restore, offline recovery of missing/corrupt databases, preservation of SQLite sidecars, invalid-candidate non-interference, rejection of stale temporary staging/decompression sidecars, preservation of invalid automatic-backup sidecars as a group, recovery rollback after an injected Windows file-move failure, Unicode database and backup paths on Windows, missing-ID update/delete without automatic backups, backup failure, explicit legacy migration, unsupported schemas, audit rollback, and the external-log mutation gate.
- Windows-only audit-log tests cover idempotent replay, recovery of an incomplete active-log tail, size-based rotation, and duplicate detection across archives.
- GitHub Actions: Linux core tests, modern Win32 compile/tests, and a separate w64devkit x86 XP-targeted build with PE architecture/subsystem/import checks and a disposable ZIP artefact.

### Remaining work

- Extend failure-injection coverage to sudden process/power loss and the case where rollback itself cannot complete; validate the recovery dialog on Windows XP.
- Add later schema migration versions as product requirements evolve; the first version 0 to 1 migration is implemented.
- Complete broader audit coverage and the remaining application workflows.
- Capture the user's manual target-machine test reproducibly (artefact/hash, exact Windows build and exercised scenarios). The user reports testing has occurred; those details are not yet recorded.
- Produce a normal portable distribution only after the remaining safety and migration work is complete.

### Test and data safety

All automated tests use generated databases and temporary backup files. The user's real database has not been copied, opened, migrated, or modified. Do not use the current prototype with real data.

## Implemented Win32 UI refinements

The following changes are now in the application source; the new CI run is the compile/test gate, and visual confirmation of resource glyphs remains a manual check:

- Settings commands are grouped under the **Opções** menu.
- Main-window manual refresh UI and its "Actualizado às ..." timestamp have been removed. List reads remain event-driven; no timer or periodic SQLite polling exists.
- Audit-log recovery is available through an explicit **Ficheiro** menu command and is no longer bound to F5.
- The main window uses a Win32 accelerator table for Ctrl+N and Ctrl+H. List activation handles Enter; Delete is handled through the list-view key notification.
- The history dialog is resizable with a minimum size, header drag-and-drop and a right-click context menu for edit/delete. Existing double-click editing remains.
- Statistics no longer has a manual refresh button or F5 reload path; it reads the complete record snapshot on open and calculates year changes from memory.
- `dialogs.rc` declares UTF-8 code page 65001 and MSVC C++ compilation is explicitly UTF-8. Statistics error messages now use a UTF-8-to-wide conversion helper.

The rendering of Portuguese resource strings still needs visual confirmation on the target Windows installation; a successful resource compilation does not guarantee glyph display.

## XP-targeted build baseline

The official w64devkit x86 release is used for the XP-targeted build. The upstream release states that the x86 package supports Windows XP and requires SSE2, matching the Pentium 4 target. CI pins release 2.10.0 by SHA-256. The executable is checked as PE32/i386 with Windows GUI subsystem 5.01; selected post-XP APIs and API-set DLL imports are rejected.

The latest documented validation baseline is commit `923b8d38c61f22e93055fe112db1f780da61f5bd` on `development`.

- [Core tests run 43720771401](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771401) succeeded: Ubuntu core tests and modern Windows SDK Win32 compile/store tests, including recovery rollback failure injection, invalid automatic-backup sidecar preservation, Unicode paths and no-op backup gating.
- [XP-targeted Win32 build run 43720771387](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771387) succeeded. It used pinned w64devkit x86, built the XP-targeted application/tests, passed all 7 CTest tests, inspected PE32/i386, GUI subsystem 5.01 and imports, and published artefact [14163967976](https://github.com/quilterrantdean/BiblioLED/actions/runs/43720771387), named `BiblioLED-XP-PoC`. Archive SHA-256: `b74248df2750df12c6b7b98262e646af65478cdd9f746055dfb25676c7b4602c`.

The Actions-history cleanup on 2026-11-14 removed 223 older runs and retained the two then-current successful validation runs. Later commits and workflows create new runs normally. The user reports having tested the application manually; exact artefact, operating-system build and scenarios have not yet been recorded.




## Latest audit and static-analysis work

- The version 1 audit-schema inspector now rejects incomplete/failed `PRAGMA table_info` execution, including a non-successful statement finalisation, rather than accepting a partial scan.
- The external audit-log scanner now uses a fixed 64 KiB read buffer and bounded event-key parser state, avoiding memory use proportional to the size of an archived log. It truncates only an incomplete active-log tail; an incomplete archive is rejected unchanged.
- Windows audit-log tests now cover an active log becoming read-only (pending events remain queued and synchronisation succeeds after permissions are restored) and an archived log ending with an incomplete line (synchronisation must fail without appending). The new Cppcheck workflow scans the portable C++11 core, backup infrastructure and corresponding portable tests; its first CI result is pending.
