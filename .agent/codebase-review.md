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
