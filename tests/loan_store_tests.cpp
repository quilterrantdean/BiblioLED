#include "biblioled/loan_store.h"

#include <cstdio>
#include <cstdlib>
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

} // namespace

int main()
{
    const std::string database = "loan-store-test-" + std::to_string(std::rand()) + ".sqlite";
    const std::string missing_backup_directory = database + ".missing-backup-directory";
    std::vector<std::string> backups;
    std::remove(database.c_str());

    biblioled::LoanStore store;
    std::string error;

    Check(store.Open(database, ".", &error), "a new isolated database should open");
    if (!store.IsOpen()) {
        std::cerr << error << std::endl;
        return 1;
    }

    std::vector<biblioled::LoanRecord> records;
    Check(ReadAll(store, &records, "an empty database should be readable"),
          "empty database read should complete");
    Check(records.empty(), "a new database should contain no records");
    Check(!store.HasSessionBackup(), "opening a database should not create a backup");

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
        std::vector<biblioled::LoanRecord> snapshot;
        ReadAll(backup_reader, &snapshot, "session backup should be readable");
        Check(snapshot.empty(), "first session backup must represent the database before insertion");
        backup_reader.Close();
    }

    biblioled::LoanRecord audio;
    audio.title = "Audiolivro de teste";
    audio.loan_date = "2099-01-11 09:15";
    audio.type = "audiolivro";
    Check(store.Insert(&audio, &error), "second insert should succeed");
    Check(store.LastSessionBackupPath() == first_backup,
          "a session should reuse its first backup for later mutations");

    Check(ReadAll(store, &records, "list should succeed after inserts"),
          "records should be readable after inserts");
    Check(records.size() == 2, "list should return both inserted records");

    biblioled::LoanRecord loaded;
    Check(store.GetById(book.id, &loaded, &error), "lookup by id should succeed");
    Check(loaded.title == book.title, "UTF-8 title should round-trip");
    Check(loaded.loan_date == book.loan_date, "date should round-trip");

    loaded.title = "Título alterado";
    loaded.type = "livro";
    Check(store.Update(loaded, &error), "update should succeed");
    Check(store.GetById(book.id, &loaded, &error), "updated record should be readable");
    Check(loaded.title == "Título alterado", "updated title should be stored");
    Check(store.LastSessionBackupPath() == first_backup,
          "update in the same session should not create a second backup");

    int count = 0;
    Check(store.CountSince("2099-01-01 00:00", &count, &error),
          "count since date should succeed");
    Check(count == 2, "future-dated records should be included in quota counts");

    store.Close();

    biblioled::LoanStore reopened;
    Check(reopened.Open(database, ".", &error), "database should reopen");
    Check(!reopened.HasSessionBackup(), "a new session should start without a backup");
    Check(reopened.GetById(book.id, &loaded, &error), "record should be readable after reopen");
    const std::string before_update = loaded.title;
    loaded.title = "Alteração na segunda sessão";
    Check(reopened.Update(loaded, &error), "update in a new session should succeed");
    Check(reopened.HasSessionBackup(), "first update in a new session should create a backup");

    const std::string second_backup = reopened.LastSessionBackupPath();
    Check(!second_backup.empty() && second_backup != first_backup,
          "new session should create a distinct timestamped backup");
    backups.push_back(second_backup);

    biblioled::LoanStore second_backup_reader;
    Check(second_backup_reader.Open(second_backup, ".", &error),
          "second session backup should open");
    if (second_backup_reader.IsOpen()) {
        biblioled::LoanRecord snapshot_record;
        Check(second_backup_reader.GetById(book.id, &snapshot_record, &error),
              "second backup should contain the pre-update record");
        Check(snapshot_record.title == before_update,
              "second backup must preserve data before the update");
        Check(second_backup_reader.GetAll(&records, &error) && records.size() == 2,
              "second backup should contain both pre-update records");
        second_backup_reader.Close();
    }

    Check(reopened.Remove(audio.id, &error), "delete should succeed");
    Check(reopened.LastSessionBackupPath() == second_backup,
          "delete in the same session should reuse the session backup");
    Check(!reopened.GetById(audio.id, &loaded, &error),
          "deleted record should no longer be found");
    Check(reopened.QuickCheck(&error), "quick_check should pass after CRUD operations");
    reopened.Close();

    biblioled::LoanStore blocked;
    const std::string blocked_database = database + ".blocked.sqlite";
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

    std::remove(database.c_str());
    std::remove((database + "-journal").c_str());
    std::remove((database + "-wal").c_str());
    std::remove((database + "-shm").c_str());
    std::remove(blocked_database.c_str());
    std::remove((blocked_database + "-journal").c_str());
    std::remove((blocked_database + "-wal").c_str());
    std::remove((blocked_database + "-shm").c_str());
    for (size_t i = 0; i < backups.size(); ++i) {
        std::remove(backups[i].c_str());
    }

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed." << std::endl;
        return 1;
    }

    std::cout << "LoanStore tests passed." << std::endl;
    return 0;
}
