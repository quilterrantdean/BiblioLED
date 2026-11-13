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

    bool Open(const std::string& database_path, std::string* error);
    void Close();
    bool IsOpen() const;

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
    void SetError(std::string* error, const std::string& message) const;

    sqlite3* db_;
};

} // namespace biblioled

#endif
