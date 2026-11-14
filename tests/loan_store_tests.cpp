#include "biblioled/loan_store.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << std::endl;
    }
}

bool Execute(sqlite3* db, const char* sql)
{
    char* message = NULL;
    const int rc = sqlite3_exec(db, sql, NULL, NULL, &message);
    if (rc != SQLITE_OK) {
        std::cerr << (message ? message : sqlite3_errmsg(db)) << std::endl;
    }
    sqlite3_free(message);
    return rc == SQLITE_OK;
}

bool CreateLegacyDatabase(const std::string& path, int version)
{
    sqlite3* db = NULL;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    bool ok = Execute(db,
        "CREATE TABLE loans ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " loan_date TEXT NOT NULL,"
        " type TEXT NOT NULL"
        ");");

    sqlite3_stmt* statement = NULL;
    if (ok) {
        ok = sqlite3_prepare_v2(
            db,
            "INSERT INTO loans (title, loan_date, type) VALUES (?1, ?2, ?3);",
            -1, &statement, NULL) == SQLITE_OK;
    }
    if (ok) {
        sqlite3_bind_text(statement, 1, "Registo legado", -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, "2024-01-15 10:30", -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, "livro", -1, SQLITE_STATIC);
        ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    if (statement) sqlite3_finalize(statement);

    char pragma[64] = {0};
    std::snprintf(pragma, sizeof(pragma), "PRAGMA user_version = %d;", version);
    if (ok) ok = Execute(db, pragma);

    const int close_rc = sqlite3_close(db);
    return ok && close_rc == SQLITE_OK;
}

bool ReadAll(biblioled::LoanStore& store,
             std::vector<biblioled::LoanRecord>* records,
             const char* message)
{
    std::string error;
    const bool ok = store.GetAll(records, &error);
    Check(ok, message);
    if (!ok) std::cerr << error << std::endl;
    return ok;
}

void RemoveDatabaseFiles(const std::string& path)
{
    std::remove(path.c_str());
    std::remove((path + "-journal").c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
}

} // namespace

int main()
{
    const std::string database =
        "loan-store-test-" + std::to_string(std::rand()) + ".sqlite";
    const std::string blocked_database = database + ".blocked.sqlite";
    const std::string legacy_database = database + ".legacy.sqlite";
    const std::string failed_migration_database = database + ".failed-migration.sqlite";
    const std::string unsupported_database = database + ".unsupported.sqlite";
    const std::string malformed_v1_database = database + ".malformed-v1.sqlite";
    const std::string missing_backup_directory = database + ".missing-backup-directory";
    const std::string invalid_backup = database + ".invalid-backup.sqlite";

    std::vector<std::string> backups;
    std::string error;

    // A new database starts at schema version 1 with an empty audit table.
    biblioled::LoanStore store;
    Check(store.Open(database, ".", &error), "a new isolated database should open");
    if (!store.IsOpen()) {
        std::cerr << error << std::endl;
        return 1;
    }
    Check(store.SchemaVersion() == 1, "new database should use schema version 1");
    Check(!store.MigrationRequired(), "new database should not require migration");

    std::vector<biblioled::LoanRecord> records;
    ReadAll(store, &records, "an empty database should be readable");
    Check(records.empty(), "a new database should contain no loan records");

    std::vector<biblioled::AuditEvent> events;
    Check(store.GetAuditEvents(&events, &error), "new audit table should be readable");
    Check(events.empty(), "new database should start with no audit events");
    Check(!store.HasSessionBackup(), "opening a database should not create a backup");

    // CRUD writes and audit rows must commit or roll back together.
    biblioled::LoanRecord book;
    book.title = "Açúcar e café";
    book.loan_date = "2099-01-10 12:30";
    book.type = "livro";
    Check(store.Insert(&book, &error), "insert should succeed");
    Check(book.id > 0, "insert should return the generated identifier");
    Check(store.HasSessionBackup(), "first mutation should create a session backup");

    const std::string first_backup = store.LastSessionBackupPath();
    Check(!first_backup.empty(), "session backup path should be reported");
    backups.push_back(first_backup);

    biblioled::LoanStore backup_reader;
    Check(backup_reader.Open(first_backup, ".", &error),
          "session backup should open as a SQLite database");
    if (backup_reader.IsOpen()) {
        ReadAll(backup_reader, &records, "session backup should be readable");
        Check(records.empty(), "first session backup must represent the database before insertion");
        Check(backup_reader.SchemaVersion() == 1,
              "session backup should preserve the schema version");
        backup_reader.Close();
    }

    biblioled::LoanRecord audio;
    audio.title = "Audiolivro de teste";
    audio.loan_date = "2099-01-11 09:15";
    audio.type = "audiolivro";
    Check(store.Insert(&audio, &error), "second insert should succeed");
    Check(store.LastSessionBackupPath() == first_backup,
          "a session should reuse its first backup for later mutations");

    Check(store.GetById(book.id, &book, &error), "lookup by id should succeed");
    Check(book.title == "Açúcar e café", "UTF-8 title should round-trip");
    Check(store.GetAll(&records, &error) && records.size() == 2,
          "list should return both inserted records");

    biblioled::LoanRecord edited = book;
    edited.title = "Título alterado";
    edited.type = "livro";
    Check(store.Update(edited, &error), "update should succeed");
    Check(store.GetById(book.id, &book, &error), "updated record should be readable");
    Check(book.title == "Título alterado", "updated title should be stored");
    Check(store.LastSessionBackupPath() == first_backup,
          "update in the same session should not create a second backup");

    Check(store.GetAuditEvents(&events, &error), "audit events should be readable after CRUD");
    Check(events.size() == 3, "two inserts and one update should create three audit events");
    if (events.size() == 3) {
        Check(events[0].action == "create" && events[0].has_after,
              "first audit event should describe record creation");
        Check(events[0].after.title == "Açúcar e café",
              "create audit event should retain the after value");
        Check(events[2].action == "update" && events[2].has_before && events[2].has_after,
              "update audit event should retain before and after values");
        Check(events[2].before.title == "Açúcar e café" &&
              events[2].after.title == "Título alterado",
              "update audit event should contain the correct before/after titles");
    }

    // A manual backup is distinct and preserves both loan and audit history.
    std::string manual_backup;
    Check(store.CreateManualBackup(&manual_backup, &error),
          "manual backup should be created and validated");
    Check(!manual_backup.empty() && manual_backup != first_backup,
          "manual backup should be a separate file");
    backups.push_back(manual_backup);

    biblioled::LoanStore manual_reader;
    Check(manual_reader.Open(manual_backup, ".", &error), "manual backup should open");
    if (manual_reader.IsOpen()) {
        Check(manual_reader.GetAll(&records, &error) && records.size() == 2,
              "manual backup should contain the current records");
        Check(manual_reader.GetAuditEvents(&events, &error) && events.size() == 3,
              "manual backup should contain the matching audit history");
        manual_reader.Close();
    }

    int count = 0;
    Check(store.CountSince("2099-01-01 00:00", &count, &error),
          "count since date should succeed");
    Check(count == 2, "future-dated records should be included in quota counts");
    store.Close();

    // A second session gets a new snapshot before its first mutation.
    biblioled::LoanStore reopened;
    Check(reopened.Open(database, ".", &error), "database should reopen");
    Check(reopened.SchemaVersion() == 1, "reopened database should retain schema version 1");
    Check(!reopened.MigrationRequired(), "reopened database should not require migration");
    Check(!reopened.HasSessionBackup(), "a new session should start without a backup");
    Check(reopened.GetById(book.id, &book, &error), "record should be readable after reopen");

    const std::string before_update = book.title;
    edited = book;
    edited.title = "Alteração na segunda sessão";
    Check(reopened.Update(edited, &error), "update in a new session should succeed");
    Check(reopened.HasSessionBackup(), "first update in a new session should create a backup");

    const std::string second_backup = reopened.LastSessionBackupPath();
    Check(!second_backup.empty() && second_backup != first_backup,
          "new session should create a distinct timestamped backup");
    backups.push_back(second_backup);

    biblioled::LoanStore second_backup_reader;
    Check(second_backup_reader.Open(second_backup, ".", &error),
          "second session backup should open");
    if (second_backup_reader.IsOpen()) {
        Check(second_backup_reader.GetById(book.id, &book, &error),
              "second backup should contain the pre-update record");
        Check(book.title == before_update,
              "second backup must preserve data before the update");
        Check(second_backup_reader.GetAuditEvents(&events, &error) && events.size() == 3,
              "second backup should contain audit events before the update");
        second_backup_reader.Close();
    }

    Check(reopened.Remove(audio.id, &error), "delete should succeed");
    Check(reopened.LastSessionBackupPath() == second_backup,
          "delete in the same session should reuse the session backup");
    Check(!reopened.GetById(audio.id, &book, &error),
          "deleted record should no longer be found");
    Check(reopened.GetAuditEvents(&events, &error) && events.size() == 5,
          "delete should append a fifth audit event");
    if (events.size() == 5) {
        Check(events[4].action == "delete" && events[4].has_before && !events[4].has_after,
              "delete audit event should retain before values only");
        Check(events[4].before.title == "Audiolivro de teste",
              "delete audit event should preserve the removed title");
    }

    // Restore returns both loans and internal audit history to the selected snapshot.
    std::string pre_restore_backup;
    Check(reopened.RestoreFromBackup(second_backup, &pre_restore_backup, &error),
          "restore from a valid backup should succeed");
    Check(!pre_restore_backup.empty() && pre_restore_backup != second_backup,
          "restore must create a separate safety copy of the current database");
    backups.push_back(pre_restore_backup);

    biblioled::LoanStore pre_restore_reader;
    Check(pre_restore_reader.Open(pre_restore_backup, ".", &error),
          "pre-restore safety copy should open");
    if (pre_restore_reader.IsOpen()) {
        Check(pre_restore_reader.GetAll(&records, &error) && records.size() == 1,
              "pre-restore safety copy should contain the state before restore");
        Check(pre_restore_reader.GetAuditEvents(&events, &error) && events.size() == 5,
              "pre-restore safety copy should retain the matching audit history");
        pre_restore_reader.Close();
    }

    Check(reopened.GetAll(&records, &error) && records.size() == 2,
          "restore should replace the database with the selected snapshot");
    Check(reopened.GetAuditEvents(&events, &error) && events.size() == 3,
          "restore should replace audit history with the snapshot history");
    Check(reopened.GetById(book.id, &book, &error),
          "restored database should contain the original record");
    Check(book.title == "Título alterado",
          "restore should recover the title captured in the selected snapshot");
    Check(reopened.GetById(audio.id, &book, &error),
          "restore should recover records removed after the selected snapshot");

    // A failed audit insert must roll back the corresponding loan insert.
    sqlite3* raw = NULL;
    Check(sqlite3_open(database.c_str(), &raw) == SQLITE_OK,
          "raw SQLite connection should open for failure injection");
    if (raw) {
        Check(Execute(raw,
            "CREATE TRIGGER fail_audit_insert BEFORE INSERT ON audit_events "
            "BEGIN SELECT RAISE(ABORT, 'audit write blocked'); END;"),
            "audit failure trigger should be created");

        biblioled::LoanRecord rejected;
        rejected.title = "Não deve ficar gravado";
        rejected.loan_date = "2099-02-01 10:00";
        rejected.type = "livro";
        Check(!reopened.Insert(&rejected, &error),
              "loan insert should fail when its audit row cannot be inserted");
        Check(rejected.id == 0, "failed insert should not expose a generated identifier");
        Check(reopened.GetAll(&records, &error) && records.size() == 2,
              "failed audit write should roll back the loan insert");
        Check(reopened.GetAuditEvents(&events, &error) && events.size() == 3,
              "failed audit write should not leave an orphan audit event");

        Check(Execute(raw, "DROP TRIGGER fail_audit_insert;"),
              "audit failure trigger should be removed");
        sqlite3_close(raw);
        raw = NULL;
    }
    if (raw) sqlite3_close(raw);

    {
        std::ofstream invalid(invalid_backup.c_str(), std::ios::out | std::ios::binary);
        invalid << "This is not a SQLite database.";
    }
    Check(!reopened.RestoreFromBackup(invalid_backup, NULL, &error),
          "restore should reject a non-SQLite file");
    Check(reopened.GetAll(&records, &error) && records.size() == 2,
          "invalid restore source must not change the current database");
    Check(reopened.QuickCheck(&error), "quick_check should pass after restore and failure injection");
    reopened.Close();

    // Legacy schema v0 is readable but write-protected until confirmed migration.
    Check(CreateLegacyDatabase(legacy_database, 0),
          "legacy version-0 database should be created");
    biblioled::LoanStore legacy;
    Check(legacy.Open(legacy_database, ".", &error),
          "legacy version-0 database should open for read-only access");
    Check(legacy.SchemaVersion() == 0, "legacy database should be detected as version 0");
    Check(legacy.MigrationRequired(), "legacy database should require explicit migration");

    Check(legacy.GetAll(&records, &error) && records.size() == 1,
          "legacy records should remain readable before migration");
    Check(records.size() == 1 && records[0].title == "Registo legado",
          "legacy record should remain unchanged before migration");

    biblioled::LoanRecord blocked_write;
    blocked_write.title = "Alteração não autorizada";
    blocked_write.loan_date = "2024-01-16 10:30";
    blocked_write.type = "livro";
    Check(!legacy.Insert(&blocked_write, &error),
          "legacy database should reject writes before migration confirmation");
    Check(legacy.GetAll(&records, &error) && records.size() == 1,
          "rejected legacy write should leave records unchanged");

    std::string migration_backup;
    Check(legacy.MigrateLegacySchema(&migration_backup, &error),
          "confirmed legacy migration should succeed");
    Check(legacy.SchemaVersion() == 1, "migration should set schema version 1");
    Check(!legacy.MigrationRequired(), "successful migration should clear read-only state");
    Check(legacy.HasSessionBackup() && legacy.LastSessionBackupPath() == migration_backup,
          "migration should retain its validated pre-migration backup");
    backups.push_back(migration_backup);

    Check(legacy.GetAll(&records, &error) && records.size() == 1,
          "migration should preserve every legacy loan record");
    Check(legacy.GetAuditEvents(&events, &error) && events.size() == 1,
          "migration should create one internal audit event");
    if (events.size() == 1) {
        Check(events[0].action == "schema_migration",
              "migration audit event should identify the migration");
        Check(events[0].details == "schema_version: 0 -> 1",
              "migration audit event should record source and target versions");
    }

    biblioled::LoanStore legacy_backup_reader;
    Check(legacy_backup_reader.Open(migration_backup, ".", &error),
          "pre-migration backup should remain openable");
    if (legacy_backup_reader.IsOpen()) {
        Check(legacy_backup_reader.SchemaVersion() == 0 &&
              legacy_backup_reader.MigrationRequired(),
              "pre-migration backup should remain at legacy schema version 0");
        Check(legacy_backup_reader.GetAll(&records, &error) && records.size() == 1,
              "pre-migration backup should preserve all legacy records");
        legacy_backup_reader.Close();
    }
    legacy.Close();

    // Failed migration backup must leave the legacy database unchanged and readable.
    Check(CreateLegacyDatabase(failed_migration_database, 0),
          "second legacy database should be created");
    biblioled::LoanStore failed_migration;
    Check(failed_migration.Open(failed_migration_database, missing_backup_directory, &error),
          "legacy database should open even when backup directory is unavailable");
    if (failed_migration.IsOpen()) {
        Check(!failed_migration.MigrateLegacySchema(NULL, &error),
              "migration should fail if its mandatory backup cannot be created");
        Check(failed_migration.SchemaVersion() == 0 && failed_migration.MigrationRequired(),
              "failed migration should leave schema version 0 and read-only state intact");
        Check(failed_migration.GetAll(&records, &error) && records.size() == 1,
              "failed migration should preserve legacy loan records");
        failed_migration.Close();
    }

    // Unknown future versions and inconsistent version-1 databases are rejected.
    Check(CreateLegacyDatabase(unsupported_database, 7),
          "unsupported-version database should be created");
    biblioled::LoanStore unsupported;
    Check(!unsupported.Open(unsupported_database, ".", &error),
          "application should reject a database schema newer than it supports");
    unsupported.Close();

    Check(CreateLegacyDatabase(malformed_v1_database, 1),
          "malformed version-1 database should be created");
    biblioled::LoanStore malformed;
    Check(!malformed.Open(malformed_v1_database, ".", &error),
          "version 1 without its audit table should be rejected");
    malformed.Close();

    // Backup-write failure still blocks mutations on current-schema databases.
    biblioled::LoanStore blocked;
    Check(blocked.Open(blocked_database, missing_backup_directory, &error),
          "database with a deliberately missing backup directory should open");
    if (blocked.IsOpen()) {
        biblioled::LoanRecord forbidden;
        forbidden.title = "Must not be inserted";
        forbidden.loan_date = "2099-02-01 10:00";
        forbidden.type = "livro";
        Check(!blocked.Insert(&forbidden, &error),
              "a failed session backup must block the database mutation");
        Check(!error.empty(), "backup failure should report an actionable error");
        Check(ReadAll(blocked, &records, "database should remain readable after backup failure"),
              "read after failed backup should succeed");
        Check(records.empty(), "failed backup must leave the database unchanged");
        blocked.Close();
    }

    RemoveDatabaseFiles(database);
    RemoveDatabaseFiles(blocked_database);
    RemoveDatabaseFiles(legacy_database);
    RemoveDatabaseFiles(failed_migration_database);
    RemoveDatabaseFiles(unsupported_database);
    RemoveDatabaseFiles(malformed_v1_database);
    RemoveDatabaseFiles(invalid_backup);
    for (size_t i = 0; i < backups.size(); ++i) {
        RemoveDatabaseFiles(backups[i]);
    }

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed." << std::endl;
        return 1;
    }

    std::cout << "LoanStore tests passed." << std::endl;
    return 0;
}
