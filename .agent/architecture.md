# Proposed architecture

This is a target design, not a description of the current Python implementation. The exact language, compiler, build system, and file layout remain subject to the Windows XP proof of concept.

## Responsibilities and dependency direction

### 1. User interface

Responsibilities:
- Native Win32 windows, menus, dialogs, tables, keyboard shortcuts, and status information.
- Main operational view of active loans and availability.
- Separate full-history window, filters, and export commands.
- Settings, statistics, About, backup, and restore UI.
- Present progress and errors without implementing business rules.

The UI calls application services/use cases. It does not issue SQL directly.

### 2. Business rules

Responsibilities:
- Daily and rolling-window loan limits.
- Active-loan and expiry calculations.
- Next-availability calculations.
- Statistics and date/time rules.
- Validation of loan operations and settings.

This component must run and be tested without creating a GUI.

### 3. Persistence

Responsibilities:
- SQLite connections and transactions.
- Repositories/queries for loans and audit records.
- Schema versioning and migrations.
- Backup creation, integrity validation, and restore support.

The current database schema is the compatibility baseline: `loans(id, title, loan_date, type)`.

### 4. Infrastructure

Responsibilities:
- Portable path resolution based on the executable location, not the process working directory.
- Settings file loading, validation, and saving.
- Diagnostic log writing and archival.
- Single-instance coordination.
- File-system operations and application lifecycle support.

The application should not depend on AppData, registry installation, Windows services, or administrator privileges.

## Suggested dependency direction

- UI -> application services/use cases.
- Application services -> business rules and persistence interfaces.
- SQLite persistence implements the persistence interfaces.
- Infrastructure provides file, settings, logging, and lifecycle services.
- Business rules should not depend on Win32, SQLite implementation details, or GUI types.

Keep the design proportionate to a small desktop utility. Avoid unnecessary frameworks, elaborate plugin systems, and excessive abstraction.

## Portable folder layout

Suggested shape (final names may change with the chosen implementation):

```text
BiblioLED/
  BiblioLED.exe
  config/
    settings file
  data/
    biblioteca.sqlite
    backups/
  exports/
  logs/
  resources/
    port.ico
```

All paths are resolved from the executable directory. The existing application icon is retained. Program updates must not overwrite user data, settings, backups, exports, or logs.

## Database safety model

- SQLite is the authoritative store.
- Database schema has an explicit version.
- Migrations are versioned, transactional, backed up, validated, and require explicit confirmation.
- Do not silently create a fresh database when an expected database cannot be opened.
- Test databases are always isolated from the user's real database.
- Backups are standalone SQLite files, uncompressed, timestamped, and retained until the user deletes them.
- A backup is not considered valid merely because a file exists: validate its SQLite integrity.

## Audit logging model

The intended design must keep database changes and their audit events consistent. The proposed approach is to persist audit events within the same SQLite transaction as the associated data change, then synchronise them to the external append-only text log. This allows recovery if the process stops between the database commit and the file write.

The interaction between disabling file logs and retaining internal audit records remains an implementation detail to settle before coding.

## Main UI and secondary windows

- Main window: active loans and current operational information; do not make the full history the main view.
- History: separate window; real-time filters; keep it responsive for long histories.
- Statistics: general/evolution and usage-pattern views; no reading-progress or streak tracking.
- About: existing icon, application name, version, short description, and build date.
- Appearance: compact native Windows utility, prioritising legibility and functional density over decoration.
