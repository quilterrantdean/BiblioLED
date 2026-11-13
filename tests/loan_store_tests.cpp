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

} // namespace

int main()
{
    const std::string database = "loan-store-test-" + std::to_string(std::rand()) + ".sqlite";
    std::remove(database.c_str());

    biblioled::LoanStore store;
    std::string error;

    Check(store.Open(database, &error), "a new isolated database should open");
    if (!store.IsOpen()) {
        std::cerr << error << std::endl;
        return 1;
    }

    std::vector<biblioled::LoanRecord> records;
    Check(store.GetAll(&records, &error), "an empty database should be readable");
    Check(records.empty(), "a new database should contain no records");

    biblioled::LoanRecord book;
    book.title = "Açúcar e café";
    book.loan_date = "2099-01-10 12:30";
    book.type = "livro";
    Check(store.Insert(&book, &error), "insert should succeed");
    Check(book.id > 0, "insert should return the generated identifier");

    biblioled::LoanRecord audio;
    audio.title = "Audiolivro de teste";
    audio.loan_date = "2099-01-11 09:15";
    audio.type = "audiolivro";
    Check(store.Insert(&audio, &error), "second insert should succeed");

    Check(store.GetAll(&records, &error), "list should succeed after inserts");
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

    int count = 0;
    Check(store.CountSince("2099-01-01 00:00", &count, &error),
          "count since date should succeed");
    Check(count == 2, "future-dated records should be included in quota counts");

    Check(store.Remove(audio.id, &error), "delete should succeed");
    Check(!store.GetById(audio.id, &loaded, &error),
          "deleted record should no longer be found");

    Check(store.QuickCheck(&error), "quick_check should pass after CRUD operations");
    store.Close();

    biblioled::LoanStore reopened;
    Check(reopened.Open(database, &error), "database should reopen");
    Check(reopened.GetAll(&records, &error), "records should remain readable after reopen");
    Check(records.size() == 1, "one record should remain after deletion");
    reopened.Close();

    std::remove(database.c_str());
    std::remove((database + "-journal").c_str());
    std::remove((database + "-wal").c_str());
    std::remove((database + "-shm").c_str());

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed." << std::endl;
        return 1;
    }

    std::cout << "LoanStore tests passed." << std::endl;
    return 0;
}
