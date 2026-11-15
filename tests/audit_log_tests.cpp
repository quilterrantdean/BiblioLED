#include "biblioled/audit_log.h"

#include <windows.h>

#include <cstdio>
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

bool MakeTemporaryDirectory(std::wstring* path)
{
    wchar_t temp[MAX_PATH] = {0};
    if (!GetTempPathW(MAX_PATH, temp)) return false;
    wchar_t name[MAX_PATH] = {0};
    if (!GetTempFileNameW(temp, L"BLG", 0, name)) return false;
    DeleteFileW(name);
    if (!CreateDirectoryW(name, NULL)) return false;
    *path = name;
    return true;
}

int CountLogFiles(const std::wstring& directory)
{
    WIN32_FIND_DATAW data;
    const std::wstring pattern = directory + L"\\audit*.log";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return 0;

    int count = 0;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++count;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return count;
}

void RemoveLogFiles(const std::wstring& directory)
{
    WIN32_FIND_DATAW data;
    const std::wstring pattern = directory + L"\\audit*.log";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                DeleteFileW((directory + L"\\" + data.cFileName).c_str());
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    RemoveDirectoryW(directory.c_str());
}

bool AppendIncompleteTail(const std::wstring& path)
{
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    static const char fragment[] = "partial event_key=not-complete";
    DWORD written = 0;
    const BOOL ok = WriteFile(file, fragment, sizeof(fragment) - 1, &written, NULL);
    const BOOL flushed = ok && written == sizeof(fragment) - 1
        ? FlushFileBuffers(file) : FALSE;
    CloseHandle(file);
    return ok && flushed;
}

} // namespace

int main()
{
    std::wstring directory;
    Check(MakeTemporaryDirectory(&directory),
          "temporary audit-log directory should be created");
    if (directory.empty()) return 1;

    std::vector<biblioled::AuditEvent> events;
    for (int i = 1; i <= 3; ++i) {
        biblioled::AuditEvent event;
        event.id = i;
        event.occurred_at = "2026-11-14 12:00:00";
        event.action = "create";
        event.loan_id = i;
        event.has_after = true;
        event.after.id = i;
        event.after.title = "Título de teste " + std::to_string(i);
        event.after.loan_date = "2026-11-14 12:00";
        event.after.type = "livro";
        events.push_back(event);
    }

    std::string error;
    int written = -1;
    Check(biblioled::SynchronizeAuditLog(directory, events, 65536, &written, &error),
          "initial audit events should be synchronised");
    Check(written == 3, "initial synchronisation should write all three events");
    Check(CountLogFiles(directory) == 1,
          "small audit history should use one active log file");

    Check(biblioled::SynchronizeAuditLog(directory, events, 65536, &written, &error),
          "repeated synchronisation should succeed");
    Check(written == 0, "already synchronised events must not be duplicated");

    // A read-only active log must block synchronisation without losing the
    // pending event. Once write access is restored, the event must be appended.
    std::wstring readonly_directory;
    Check(MakeTemporaryDirectory(&readonly_directory),
          "read-only audit-log test directory should be created");
    if (!readonly_directory.empty()) {
        std::vector<biblioled::AuditEvent> readonly_events;
        readonly_events.push_back(events[0]);
        Check(biblioled::SynchronizeAuditLog(
                  readonly_directory, readonly_events, 65536, &written, &error),
              "read-only test should first create a valid active log");

        biblioled::AuditEvent pending = events[0];
        pending.id = 4;
        pending.loan_id = 4;
        pending.after.id = 4;
        pending.after.title = "Evento pendente";
        readonly_events.push_back(pending);

        const std::wstring active_path = readonly_directory + L"\\audit.log";
        Check(SetFileAttributesW(active_path.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE,
              "active audit log should become read-only");
        error.clear();
        written = -1;
        Check(!biblioled::SynchronizeAuditLog(
                  readonly_directory, readonly_events, 65536, &written, &error),
              "a read-only active log should block synchronisation");
        Check(written == 0 && !error.empty(),
              "a blocked audit synchronisation should report failure and write no events");
        Check(SetFileAttributesW(active_path.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
              "active audit log should become writable again");
        error.clear();
        Check(biblioled::SynchronizeAuditLog(
                  readonly_directory, readonly_events, 65536, &written, &error),
              "synchronisation should recover after active-log write access returns");
        Check(written == 1,
              "the event blocked by read-only permissions should be appended exactly once");
        RemoveLogFiles(readonly_directory);
    }

    Check(AppendIncompleteTail(directory + L"\\audit.log"),
          "incomplete tail should be appended for recovery test");
    Check(biblioled::SynchronizeAuditLog(directory, events, 65536, &written, &error),
          "synchronisation should recover an incomplete final line");
    Check(written == 0, "recovery must not duplicate complete events");

    std::wstring rotation_directory;
    Check(MakeTemporaryDirectory(&rotation_directory),
          "rotation test directory should be created");
    if (!rotation_directory.empty()) {
        std::vector<biblioled::AuditEvent> large_events;
        for (int i = 10; i < 14; ++i) {
            biblioled::AuditEvent event;
            event.id = i;
            event.occurred_at = "2026-11-14 12:30:00";
            event.action = "update";
            event.loan_id = i;
            event.has_before = true;
            event.has_after = true;
            event.before.title = "Antes";
            event.before.loan_date = "2026-11-13 12:30";
            event.before.type = "livro";
            event.after.title = std::string(40000, static_cast<char>('A' + (i - 10)));
            event.after.loan_date = "2026-11-14 12:30";
            event.after.type = "livro";
            large_events.push_back(event);
        }

        Check(biblioled::SynchronizeAuditLog(
                  rotation_directory, large_events, 65536, &written, &error),
              "large audit events should synchronise with rotation");
        Check(written == 4, "rotation test should write all four events");
        Check(CountLogFiles(rotation_directory) >= 2,
              "the configured size threshold should create archived log files");

        Check(biblioled::SynchronizeAuditLog(
                  rotation_directory, large_events, 65536, &written, &error),
              "events in active and archived logs should be recognised");
        Check(written == 0,
              "events in archived logs must not be appended again");

        // Archived files are immutable history. Unlike the active log, an
        // incomplete archived line must be reported rather than truncated.
        WIN32_FIND_DATAW archived_data;
        const std::wstring archived_pattern = rotation_directory + L"\\audit_*.log";
        HANDLE archived_find = FindFirstFileW(archived_pattern.c_str(), &archived_data);
        std::wstring archived_path;
        if (archived_find != INVALID_HANDLE_VALUE) {
            do {
                if (!(archived_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    archived_path = rotation_directory + L"\\" + archived_data.cFileName;
                    break;
                }
            } while (FindNextFileW(archived_find, &archived_data));
            FindClose(archived_find);
        }
        Check(!archived_path.empty(),
              "rotation should produce an archived log for corruption testing");
        if (!archived_path.empty()) {
            Check(AppendIncompleteTail(archived_path),
                  "an incomplete archived-log tail should be appended for testing");
            error.clear();
            written = -1;
            Check(!biblioled::SynchronizeAuditLog(
                      rotation_directory, large_events, 65536, &written, &error),
                  "an archived log with an incomplete final line should block synchronisation");
            Check(written == 0 && !error.empty(),
                  "archived-log corruption should report failure without appending new events");
        }

        RemoveLogFiles(rotation_directory);
    }

    RemoveLogFiles(directory);

    if (failures != 0) {
        std::cerr << failures << " audit-log assertion(s) failed." << std::endl;
        return 1;
    }

    std::cout << "Audit log tests passed." << std::endl;
    return 0;
}
