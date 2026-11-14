#include "biblioled/loan_store.h"

#include <sqlite3.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace biblioled {
namespace {

bool FileExists(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    return file.good();
}

std::string JoinPath(const std::string& directory, const std::string& name)
{
    if (directory.empty()) return name;
    const char last = directory[directory.size() - 1];
    if (last == '/' || last == '\\') return directory + name;
    return directory + "/" + name;
}

bool HasColumn(sqlite3* db, const char* column_name)
{
    sqlite3_stmt* statement = NULL;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(loans);", -1, &statement, NULL) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(statement, 1);
        if (name && sqlite3_stricmp(reinterpret_cast<const char*>(name), column_name) == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    return found;
}

bool ValidateDatabaseFile(const std::string& path, std::string* error)
{
    sqlite3* check_db = NULL;
    int rc = sqlite3_open_v2(path.c_str(), &check_db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (error) {
            *error = check_db ? sqlite3_errmsg(check_db) : "Could not open backup for validation.";
        }
        if (check_db) sqlite3_close(check_db);
        return false;
    }

    sqlite3_stmt* statement = NULL;
    rc = sqlite3_prepare_v2(check_db, "PRAGMA integrity_check;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(check_db);
        sqlite3_close(check_db);
        return false;
    }

    bool saw_result = false;
    bool valid = true;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        saw_result = true;
        const unsigned char* result = sqlite3_column_text(statement, 0);
        if (!result || sqlite3_stricmp(reinterpret_cast<const char*>(result), "ok") != 0) {
            valid = false;
            if (error) {
                *error = result
                    ? reinterpret_cast<const char*>(result)
                    : "SQLite integrity_check returned an empty result.";
            }
        }
    }

    if (rc != SQLITE_DONE) {
        valid = false;
        if (error) *error = sqlite3_errmsg(check_db);
    }
    if (!saw_result) {
        valid = false;
        if (error) *error = "SQLite integrity_check returned no result.";
    }

    sqlite3_finalize(statement);
    const int close_rc = sqlite3_close(check_db);
    if (close_rc != SQLITE_OK) {
        valid = false;
        if (error) *error = "Could not close the backup after validation.";
    }
    return valid;
}

std::string SqliteError(sqlite3* db, const std::string& operation)
{
    std::string message = operation;
    message += ": ";
    message += db ? sqlite3_errmsg(db) : "SQLite database is not open";
    return message;
}

} // namespace

LoanStore::LoanStore()
    : db_(NULL),
      session_backup_done_(false)
{
}

LoanStore::~LoanStore()
{
    Close();
}

void LoanStore::SetError(std::string* error, const std::string& message) const
{
    if (error) *error = message;
}

bool LoanStore::Open(const std::string& database_path,
                     const std::string& backup_directory,
                     std::string* error)
{
    Close();

    database_path_ = database_path;
    backup_directory_ = backup_directory;
    session_backup_done_ = false;
    last_session_backup_path_.clear();

    const bool existed = FileExists(database_path);
    const int flags = SQLITE_OPEN_READWRITE | (existed ? 0 : SQLITE_OPEN_CREATE);

    int rc = sqlite3_open_v2(database_path.c_str(), &db_, flags, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to open database"));
        Close();
        return false;
    }

    sqlite3_busy_timeout(db_, 3000);

    if (!QuickCheck(error)) {
        Close();
        return false;
    }

    if (existed) {
        if (!ValidateExistingSchema(error)) {
            Close();
            return false;
        }
    } else {
        if (!CreateInitialSchema(error)) {
            Close();
            return false;
        }
    }

    return true;
}

void LoanStore::Close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = NULL;
    }
    database_path_.clear();
    backup_directory_.clear();
    last_session_backup_path_.clear();
    session_backup_done_ = false;
}

bool LoanStore::IsOpen() const
{
    return db_ != NULL;
}

bool LoanStore::HasSessionBackup() const
{
    return session_backup_done_;
}

const std::string& LoanStore::LastSessionBackupPath() const
{
    return last_session_backup_path_;
}

std::string LoanStore::MakeBackupPath() const
{
    const std::time_t now = std::time(NULL);
    const std::tm* local = std::localtime(&now);
    if (!local) return std::string();

    char timestamp[32] = {0};
    if (std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", local) == 0) {
        return std::string();
    }

    for (int suffix = 0; suffix < 1000; ++suffix) {
        std::ostringstream filename;
        filename << "BiblioLED_" << timestamp;
        if (suffix > 0) filename << "_" << suffix;
        filename << ".sqlite";

        const std::string candidate = JoinPath(backup_directory_, filename.str());
        if (!FileExists(candidate)) return candidate;
    }
    return std::string();
}

bool LoanStore::CreateAndValidateBackup(const std::string& path, std::string* error)
{
    sqlite3* backup_db = NULL;
    int rc = sqlite3_open_v2(
        path.c_str(), &backup_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(backup_db, "Could not create the session backup"));
        if (backup_db) sqlite3_close(backup_db);
        std::remove(path.c_str());
        return false;
    }

    sqlite3_busy_timeout(backup_db, 3000);
    sqlite3_backup* backup = sqlite3_backup_init(backup_db, "main", db_, "main");
    if (!backup) {
        SetError(error, SqliteError(backup_db, "Could not initialise the SQLite backup"));
        sqlite3_close(backup_db);
        std::remove(path.c_str());
        return false;
    }

    rc = SQLITE_OK;
    int attempts = 0;
    while (attempts < 200) {
        rc = sqlite3_backup_step(backup, -1);
        if (rc == SQLITE_DONE) break;
        if (rc == SQLITE_OK) continue;
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            ++attempts;
            sqlite3_sleep(25);
            continue;
        }
        break;
    }

    const int finish_rc = sqlite3_backup_finish(backup);
    const std::string backup_error = sqlite3_errmsg(backup_db);
    const int close_rc = sqlite3_close(backup_db);

    if (rc != SQLITE_DONE || finish_rc != SQLITE_OK || close_rc != SQLITE_OK) {
        std::string message = "SQLite backup failed";
        if (!backup_error.empty()) message += ": " + backup_error;
        SetError(error, message);
        std::remove(path.c_str());
        return false;
    }

    std::string validation_error;
    if (!ValidateDatabaseFile(path, &validation_error)) {
        SetError(error, "The session backup failed integrity validation: " + validation_error);
        std::remove(path.c_str());
        return false;
    }
    return true;
}

bool LoanStore::EnsureSessionBackup(std::string* error)
{
    if (session_backup_done_) return true;
    if (backup_directory_.empty()) {
        SetError(error, "The backup directory is not configured. The database was not changed.");
        return false;
    }

    const std::string path = MakeBackupPath();
    if (path.empty()) {
        SetError(error, "Could not choose a unique name for the session backup. The database was not changed.");
        return false;
    }

    if (!CreateAndValidateBackup(path, error)) {
        if (error && !error->empty()) {
            *error += ". The database was not changed.";
        }
        return false;
    }

    last_session_backup_path_ = path;
    session_backup_done_ = true;
    return true;
}

bool LoanStore::CreateInitialSchema(std::string* error)
{
    static const char* sql =
        "CREATE TABLE loans ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " loan_date TEXT NOT NULL,"
        " type TEXT NOT NULL"
        ");";

    char* message = NULL;
    const int rc = sqlite3_exec(db_, sql, NULL, NULL, &message);
    if (rc != SQLITE_OK) {
        const std::string detail = message ? message : sqlite3_errmsg(db_);
        sqlite3_free(message);
        SetError(error, "Unable to create the initial database schema: " + detail);
        return false;
    }
    return true;
}

bool LoanStore::ValidateExistingSchema(std::string* error)
{
    if (!HasColumn(db_, "id") ||
        !HasColumn(db_, "title") ||
        !HasColumn(db_, "loan_date") ||
        !HasColumn(db_, "type")) {
        SetError(error,
            "The existing database does not contain the expected loans schema. "
            "It has not been modified.");
        return false;
    }
    return true;
}

bool LoanStore::QuickCheck(std::string* error)
{
    if (!db_) {
        SetError(error, "Database is not open.");
        return false;
    }

    sqlite3_stmt* statement = NULL;
    int rc = sqlite3_prepare_v2(db_, "PRAGMA quick_check;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "SQLite quick_check failed"));
        return false;
    }

    bool valid = false;
    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        const unsigned char* result = sqlite3_column_text(statement, 0);
        valid = result && sqlite3_stricmp(reinterpret_cast<const char*>(result), "ok") == 0;
    }

    sqlite3_finalize(statement);
    if (!valid) {
        SetError(error, SqliteError(db_, "Database integrity check failed"));
        return false;
    }
    return true;
}

bool LoanStore::GetAll(std::vector<LoanRecord>* records, std::string* error)
{
    if (!db_ || !records) {
        SetError(error, "Invalid database or output argument.");
        return false;
    }

    records->clear();
    sqlite3_stmt* statement = NULL;
    const char* sql =
        "SELECT id, title, loan_date, type "
        "FROM loans ORDER BY loan_date DESC, id DESC;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan query"));
        return false;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        LoanRecord record;
        record.id = sqlite3_column_int(statement, 0);

        const unsigned char* title = sqlite3_column_text(statement, 1);
        const unsigned char* loan_date = sqlite3_column_text(statement, 2);
        const unsigned char* type = sqlite3_column_text(statement, 3);

        record.title = title ? reinterpret_cast<const char*>(title) : "";
        record.loan_date = loan_date ? reinterpret_cast<const char*>(loan_date) : "";
        record.type = type ? reinterpret_cast<const char*>(type) : "";
        records->push_back(record);
    }

    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        SetError(error, SqliteError(db_, "Unable to read loans"));
        return false;
    }
    return true;
}

bool LoanStore::GetById(int id, LoanRecord* record, std::string* error)
{
    if (!db_ || !record) {
        SetError(error, "Invalid database or output argument.");
        return false;
    }

    sqlite3_stmt* statement = NULL;
    const char* sql =
        "SELECT id, title, loan_date, type FROM loans WHERE id = ?1;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan lookup"));
        return false;
    }

    sqlite3_bind_int(statement, 1, id);
    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        record->id = sqlite3_column_int(statement, 0);
        const unsigned char* title = sqlite3_column_text(statement, 1);
        const unsigned char* loan_date = sqlite3_column_text(statement, 2);
        const unsigned char* type = sqlite3_column_text(statement, 3);
        record->title = title ? reinterpret_cast<const char*>(title) : "";
        record->loan_date = loan_date ? reinterpret_cast<const char*>(loan_date) : "";
        record->type = type ? reinterpret_cast<const char*>(type) : "";
        sqlite3_finalize(statement);
        return true;
    }

    sqlite3_finalize(statement);
    if (rc == SQLITE_DONE) {
        SetError(error, "Loan record was not found.");
    } else {
        SetError(error, SqliteError(db_, "Unable to read loan record"));
    }
    return false;
}

bool LoanStore::Insert(LoanRecord* record, std::string* error)
{
    if (!db_ || !record || record->title.empty() || record->loan_date.empty() || record->type.empty()) {
        SetError(error, "A valid database, title, date and type are required.");
        return false;
    }

    if (!EnsureSessionBackup(error)) return false;

    sqlite3_stmt* statement = NULL;
    const char* sql =
        "INSERT INTO loans (title, loan_date, type) VALUES (?1, ?2, ?3);";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan insertion"));
        return false;
    }

    sqlite3_bind_text(statement, 1, record->title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, record->loan_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, record->type.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        SetError(error, SqliteError(db_, "Unable to insert loan"));
        return false;
    }

    record->id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    return true;
}

bool LoanStore::Update(const LoanRecord& record, std::string* error)
{
    if (!db_ || record.id <= 0 || record.title.empty() ||
        record.loan_date.empty() || record.type.empty()) {
        SetError(error, "A valid record is required for update.");
        return false;
    }

    if (!EnsureSessionBackup(error)) return false;

    sqlite3_stmt* statement = NULL;
    const char* sql =
        "UPDATE loans SET title = ?1, loan_date = ?2, type = ?3 WHERE id = ?4;";

    int rc = sqlite3_prepare_v2(db_, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan update"));
        return false;
    }

    sqlite3_bind_text(statement, 1, record.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, record.loan_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, record.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, record.id);

    rc = sqlite3_step(statement);
    const int changed = sqlite3_changes(db_);
    sqlite3_finalize(statement);

    if (rc != SQLITE_DONE) {
        SetError(error, SqliteError(db_, "Unable to update loan"));
        return false;
    }
    if (changed == 0) {
        SetError(error, "Loan record was not found.");
        return false;
    }
    return true;
}

bool LoanStore::Remove(int id, std::string* error)
{
    if (!db_ || id <= 0) {
        SetError(error, "A valid record identifier is required.");
        return false;
    }

    if (!EnsureSessionBackup(error)) return false;

    sqlite3_stmt* statement = NULL;
    int rc = sqlite3_prepare_v2(db_, "DELETE FROM loans WHERE id = ?1;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan deletion"));
        return false;
    }

    sqlite3_bind_int(statement, 1, id);
    rc = sqlite3_step(statement);
    const int changed = sqlite3_changes(db_);
    sqlite3_finalize(statement);

    if (rc != SQLITE_DONE) {
        SetError(error, SqliteError(db_, "Unable to delete loan"));
        return false;
    }
    if (changed == 0) {
        SetError(error, "Loan record was not found.");
        return false;
    }
    return true;
}

bool LoanStore::CountSince(const std::string& date_time, int* count, std::string* error)
{
    if (!db_ || !count) {
        SetError(error, "Invalid database or output argument.");
        return false;
    }

    sqlite3_stmt* statement = NULL;
    int rc = sqlite3_prepare_v2(
        db_, "SELECT COUNT(*) FROM loans WHERE loan_date >= ?1;", -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        SetError(error, SqliteError(db_, "Unable to prepare loan count"));
        return false;
    }

    sqlite3_bind_text(statement, 1, date_time.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        SetError(error, SqliteError(db_, "Unable to count loans"));
        sqlite3_finalize(statement);
        return false;
    }

    *count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return true;
}

} // namespace biblioled
