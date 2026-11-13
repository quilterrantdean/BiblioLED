#include "biblioled/loan_store.h"

#include <sqlite3.h>

#include <fstream>
#include <sstream>

namespace biblioled {
namespace {

bool FileExists(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    return file.good();
}

std::string SqliteError(sqlite3* db, const std::string& operation)
{
    std::string message = operation;
    message += ": ";
    message += db ? sqlite3_errmsg(db) : "SQLite database is not open";
    return message;
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

} // namespace

LoanStore::LoanStore() : db_(NULL)
{
}

LoanStore::~LoanStore()
{
    Close();
}

void LoanStore::SetError(std::string* error, const std::string& message) const
{
    if (error) {
        *error = message;
    }
}

bool LoanStore::Open(const std::string& database_path, std::string* error)
{
    Close();

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
}

bool LoanStore::IsOpen() const
{
    return db_ != NULL;
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
