#ifndef BIBLIOLED_LOAN_STORE_H
#define BIBLIOLED_LOAN_STORE_H

#include <string>
#include <vector>

struct sqlite3;

namespace biblioled {

struct LoanRecord {
    int id;
    std::string title;
    std::string loan_date;
    std::string type;

    LoanRecord() : id(0) {}
};

class LoanStore {
public:
    LoanStore();
    ~LoanStore();

    LoanStore(const LoanStore&) = delete;
    LoanStore& operator=(const LoanStore&) = delete;

    bool Open(const std::string& database_path,
              const std::string& backup_directory,
              std::string* error);
    void Close();
    bool IsOpen() const;
    bool CreateManualBackup(std::string* backup_path, std::string* error);
    bool RestoreFromBackup(const std::string& backup_path,
                           std::string* current_backup_path,
                           std::string* error);

    bool HasSessionBackup() const;
    const std::string& LastSessionBackupPath() const;

    bool GetAll(std::vector<LoanRecord>* records, std::string* error);
    bool GetById(int id, LoanRecord* record, std::string* error);
    bool Insert(LoanRecord* record, std::string* error);
    bool Update(const LoanRecord& record, std::string* error);
    bool Remove(int id, std::string* error);
    bool CountSince(const std::string& date_time, int* count, std::string* error);
    bool QuickCheck(std::string* error);

private:
    bool CreateInitialSchema(std::string* error);
    bool ValidateExistingSchema(std::string* error);
    bool EnsureSessionBackup(std::string* error);
    bool CreateAndValidateBackup(const std::string& path, std::string* error);
    std::string MakeBackupPath() const;
    void SetError(std::string* error, const std::string& message) const;

    sqlite3* db_;
    std::string database_path_;
    std::string backup_directory_;
    std::string last_session_backup_path_;
    bool session_backup_done_;
};

} // namespace biblioled

#endif
