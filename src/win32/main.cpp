#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "biblioled/loan_store.h"
#include "biblioled/audit_log.h"
#include "resource.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef BIBLIOLED_VERSION
#define BIBLIOLED_VERSION "0.1.0"
#endif

namespace {

const wchar_t* kWindowClass = L"BiblioLEDMainWindow";
const UINT kActivateMessage = WM_APP + 10;
const UINT kPersistColumnsMessage = WM_APP + 11;
const int kColumnCount = 5;

struct AppSettings {
    int daily_limit;
    int monthly_limit;
    int monthly_window_days;
    int loan_duration_days;
    bool diagnostics_enabled;
    unsigned long audit_log_max_size_bytes;
    int column_order[kColumnCount];
    int column_widths[kColumnCount];

    AppSettings()
        : daily_limit(5), monthly_limit(20),
          monthly_window_days(30), loan_duration_days(21),
          diagnostics_enabled(true), audit_log_max_size_bytes(5UL * 1024UL * 1024UL)
    {
        for (int i = 0; i < kColumnCount; ++i) column_order[i] = i;
        column_widths[0] = 320;
        column_widths[1] = 100;
        column_widths[2] = 130;
        column_widths[3] = 130;
        column_widths[4] = 55;
    }
};

struct RecordDialogContext {
    biblioled::LoanRecord record;
    bool accepted;
    RecordDialogContext() : accepted(false) {}
};

struct VisibleLoan {
    biblioled::LoanRecord record;
    std::time_t expiry;
};

HINSTANCE gInstance = NULL;
HWND gMainWindow = NULL;
HWND gLoanList = NULL;
HWND gQuotaLabel = NULL;
HWND gStatusLabel = NULL;
HANDLE gInstanceMutex = NULL;
HICON gApplicationIcon = NULL;
std::wstring gApplicationDirectory;
std::wstring gSettingsPath;
bool gSettingsFileValid = false;
bool gColumnSettingsWarningShown = false;
biblioled::LoanStore gStore;
AppSettings gSettings;

void UpdateWriteAccess(HWND window);

enum {
    kToolbarHeight = 34,
    kQuotaHeight = 24,
    kStatusHeight = 23
};

std::wstring JoinPath(const std::wstring& left, const std::wstring& right)
{
    if (left.empty()) return right;
    const wchar_t last = left[left.size() - 1];
    if (last == L'\\' || last == L'/') return left + right;
    return left + L"\\" + right;
}

std::wstring GetApplicationDirectory()
{
    wchar_t path[MAX_PATH] = {0};
    const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L".";

    std::wstring full(path, length);
    const std::wstring::size_type separator = full.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return L".";
    return full.substr(0, separator);
}

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty()) return std::string();
    const int length = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), -1, NULL, 0, NULL, NULL);
    if (length <= 1) return std::string();

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], length, NULL, NULL);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty()) return std::wstring();
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, NULL, 0);
    if (length <= 1) return std::wstring();

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], length);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

bool EnsureDirectory(const std::wstring& path)
{
    if (CreateDirectoryW(path.c_str(), NULL)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool FileExistsW(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool CreateDatabaseMarker(const std::wstring& path, std::wstring* error)
{
    HANDLE marker = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (marker == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_EXISTS && FileExistsW(path)) return true;
        if (error) {
            *error = L"Não foi possível criar o marcador de segurança da base de dados. "
                     L"A aplicação não permitirá alterações nesta sessão.";
        }
        return false;
    }

    static const char contents[] = "BiblioLED database initialized\r\n";
    DWORD written = 0;
    const BOOL wrote = WriteFile(
        marker, contents, static_cast<DWORD>(sizeof(contents) - 1), &written, NULL);
    const BOOL flushed = wrote && written == sizeof(contents) - 1
        ? FlushFileBuffers(marker) : FALSE;
    CloseHandle(marker);

    if (!wrote || written != sizeof(contents) - 1 || !flushed) {
        DeleteFileW(path.c_str());
        if (error) {
            *error = L"Não foi possível gravar o marcador de segurança da base de dados. "
                     L"A aplicação não permitirá alterações nesta sessão.";
        }
        return false;
    }
    return true;
}

std::string Trim(const std::string& value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool ParsePositiveInteger(const std::string& text, int minimum, int maximum, int* value)
{
    if (!value) return false;
    char* end = NULL;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || !end || *end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool ParseIntegerList(const std::string& text, int minimum, int maximum,
                      int* values, int count, bool require_unique)
{
    if (!values || count <= 0) return false;
    std::string::size_type start = 0;
    for (int i = 0; i < count; ++i) {
        const std::string::size_type comma = text.find(',', start);
        if ((i < count - 1 && comma == std::string::npos) ||
            (i == count - 1 && comma != std::string::npos)) return false;
        const std::string token = Trim(text.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!ParsePositiveInteger(token, minimum, maximum, &values[i])) return false;
        if (require_unique) {
            for (int j = 0; j < i; ++j) {
                if (values[j] == values[i]) return false;
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

std::string IntegerListToString(const int* values, int count)
{
    std::ostringstream output;
    for (int i = 0; i < count; ++i) {
        if (i) output << ',';
        output << values[i];
    }
    return output.str();
}

bool ParseBoolean(const std::string& text, bool* value)
{
    if (!value) return false;
    if (text == "0") { *value = false; return true; }
    if (text == "1") { *value = true; return true; }
    return false;
}

bool WriteDefaultSettings(const std::wstring& path)
{
    const std::string utf8_path = WideToUtf8(path);
    std::ofstream output(utf8_path.c_str(), std::ios::out | std::ios::binary);
    if (!output) return false;
    output << "; BiblioLED portable settings\r\n"
              "[limits]\r\n"
              "daily_limit=5\r\n"
              "monthly_limit=20\r\n"
              "monthly_window_days=30\r\n"
              "loan_duration_days=21\r\n"
              "\r\n"
              "[diagnostics]\r\n"
              "enabled=1\r\n"
              "max_log_size_bytes=5242880\r\n"
              "\r\n"
              "[columns]\r\n"
              "order=0,1,2,3,4\r\n"
              "widths=320,100,130,130,55\r\n";
    return output.good();
}

bool LoadSettings(const std::wstring& path, AppSettings* settings, std::string* error)
{
    if (!settings) return false;

    const std::string utf8_path = WideToUtf8(path);
    std::ifstream input(utf8_path.c_str(), std::ios::in | std::ios::binary);
    if (!input.good()) {
        if (!WriteDefaultSettings(path)) {
            if (error) *error = "Não foi possível criar config/settings.ini.";
            return false;
        }
        *settings = AppSettings();
        return true;
    }

    AppSettings loaded;
    std::string line;
    int seen_limits = 0;
    bool valid = true;

    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;

        const std::string::size_type equals = line.find('=');
        if (equals == std::string::npos) { valid = false; break; }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        int parsed = 0;

        if (key == "daily_limit") {
            valid = ParsePositiveInteger(value, 1, 1000, &parsed);
            if (valid) loaded.daily_limit = parsed;
            ++seen_limits;
        } else if (key == "monthly_limit") {
            valid = ParsePositiveInteger(value, 1, 10000, &parsed);
            if (valid) loaded.monthly_limit = parsed;
            ++seen_limits;
        } else if (key == "monthly_window_days") {
            valid = ParsePositiveInteger(value, 1, 3660, &parsed);
            if (valid) loaded.monthly_window_days = parsed;
            ++seen_limits;
        } else if (key == "loan_duration_days") {
            valid = ParsePositiveInteger(value, 1, 3660, &parsed);
            if (valid) loaded.loan_duration_days = parsed;
            ++seen_limits;
        } else if (key == "enabled") {
            valid = ParseBoolean(value, &loaded.diagnostics_enabled);
        } else if (key == "max_log_size_bytes") {
            int log_size = 0;
            valid = ParsePositiveInteger(value, 65536, 1073741824, &log_size);
            if (valid) loaded.audit_log_max_size_bytes = static_cast<unsigned long>(log_size);
        } else if (key == "order") {
            valid = ParseIntegerList(value, 0, kColumnCount - 1,
                                     loaded.column_order, kColumnCount, true);
        } else if (key == "widths") {
            valid = ParseIntegerList(value, 30, 2000,
                                     loaded.column_widths, kColumnCount, false);
        }

        if (!valid) break;
    }

    if (!valid || seen_limits != 4) {
        if (error) {
            *error = "As configurações são inválidas. O ficheiro original foi preservado; "
                     "serão usados valores predefinidos nesta sessão.";
        }
        *settings = AppSettings();
        return false;
    }

    *settings = loaded;
    return true;
}

bool WriteSettings(const std::wstring& path,
                   const AppSettings& settings,
                   std::string* error)
{
    const std::wstring temporary_path = path + L".tmp";
    const std::string temporary_utf8 = WideToUtf8(temporary_path);
    {
        std::ofstream output(temporary_utf8.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "Não foi possível criar o ficheiro temporário de configurações.";
            return false;
        }
        output << "; BiblioLED portable settings\r\n"
                  "[limits]\r\n"
               << "daily_limit=" << settings.daily_limit << "\r\n"
               << "monthly_limit=" << settings.monthly_limit << "\r\n"
               << "monthly_window_days=" << settings.monthly_window_days << "\r\n"
               << "loan_duration_days=" << settings.loan_duration_days << "\r\n"
                  "\r\n"
                  "[diagnostics]\r\n"
               << "enabled=" << (settings.diagnostics_enabled ? 1 : 0) << "\r\n"
               << "max_log_size_bytes=" << settings.audit_log_max_size_bytes << "\r\n"
                  "\r\n"
                  "[columns]\r\n"
               << "order=" << IntegerListToString(settings.column_order, kColumnCount) << "\r\n"
               << "widths=" << IntegerListToString(settings.column_widths, kColumnCount) << "\r\n";
        output.flush();
        if (!output.good()) {
            if (error) *error = "Não foi possível gravar todas as configurações.";
            return false;
        }
    }

    if (!MoveFileExW(temporary_path.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (error) *error = "Não foi possível substituir config/settings.ini.";
        return false;
    }
    return true;
}

std::time_t ParseDateTime(const std::string& value)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int consumed = 0;

    if (std::sscanf(value.c_str(), "%d-%d-%d %d:%d%n",
                    &year, &month, &day, &hour, &minute, &consumed) != 5 ||
        consumed != static_cast<int>(value.size())) {
        return static_cast<std::time_t>(-1);
    }

    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return static_cast<std::time_t>(-1);
    }

    std::tm parts = {};
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_isdst = -1;

    const std::time_t result = std::mktime(&parts);
    if (result == static_cast<std::time_t>(-1)) return result;

    const std::tm* checked = std::localtime(&result);
    if (!checked ||
        checked->tm_year != year - 1900 ||
        checked->tm_mon != month - 1 ||
        checked->tm_mday != day ||
        checked->tm_hour != hour ||
        checked->tm_min != minute) {
        return static_cast<std::time_t>(-1);
    }

    return result;
}

std::string FormatDateTime(std::time_t value, const char* format)
{
    const std::tm* local = std::localtime(&value);
    if (!local) return std::string();

    char buffer[64] = {0};
    if (std::strftime(buffer, sizeof(buffer), format, local) == 0) return std::string();
    return buffer;
}

std::time_t AddDays(std::time_t value, int days)
{
    const std::tm* current = std::localtime(&value);
    if (!current) return static_cast<std::time_t>(-1);

    std::tm adjusted = *current;
    adjusted.tm_mday += days;
    adjusted.tm_isdst = -1;
    return std::mktime(&adjusted);
}

std::string CurrentDateTime()
{
    return FormatDateTime(std::time(NULL), "%Y-%m-%d %H:%M");
}

std::wstring GetControlText(HWND dialog, int control_id)
{
    wchar_t buffer[512] = {0};
    GetDlgItemTextW(dialog, control_id, buffer, 511);
    return buffer;
}

INT_PTR CALLBACK LoanDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    RecordDialogContext* context =
        reinterpret_cast<RecordDialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));

    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<RecordDialogContext*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));

        SendDlgItemMessageW(dialog, IDC_LOAN_TYPE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Livro"));
        SendDlgItemMessageW(dialog, IDC_LOAN_TYPE, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(L"Audiolivro"));

        if (context && context->record.id > 0) {
            SetDlgItemTextW(dialog, IDC_LOAN_TITLE,
                            Utf8ToWide(context->record.title).c_str());

            const int type_index =
                (context->record.type == "audiolivro" || context->record.type == "Audiolivro") ? 1 : 0;
            SendDlgItemMessageW(dialog, IDC_LOAN_TYPE, CB_SETCURSEL, type_index, 0);

            const std::string::size_type separator = context->record.loan_date.find(' ');
            const std::string date = context->record.loan_date.substr(0, separator);
            const std::string time = separator == std::string::npos
                ? "00:00" : context->record.loan_date.substr(separator + 1);

            SetDlgItemTextW(dialog, IDC_LOAN_DATE, Utf8ToWide(date).c_str());
            SetDlgItemTextW(dialog, IDC_LOAN_TIME, Utf8ToWide(time).c_str());
        } else {
            SetDlgItemTextW(dialog, IDC_LOAN_TYPE, L"Livro");
            SendDlgItemMessageW(dialog, IDC_LOAN_TYPE, CB_SETCURSEL, 0, 0);

            const std::string now = CurrentDateTime();
            SetDlgItemTextW(dialog, IDC_LOAN_DATE,
                Utf8ToWide(now.substr(0, 10)).c_str());
            SetDlgItemTextW(dialog, IDC_LOAN_TIME,
                Utf8ToWide(now.substr(11, 5)).c_str());
        }

        SetFocus(GetDlgItem(dialog, IDC_LOAN_TITLE));
        return FALSE;
    }

    if (message == WM_COMMAND && LOWORD(wParam) == IDOK) {
        if (!context) return TRUE;

        const std::wstring title = GetControlText(dialog, IDC_LOAN_TITLE);
        const std::wstring date = GetControlText(dialog, IDC_LOAN_DATE);
        const std::wstring time = GetControlText(dialog, IDC_LOAN_TIME);
        const LRESULT type_index =
            SendDlgItemMessageW(dialog, IDC_LOAN_TYPE, CB_GETCURSEL, 0, 0);

        if (title.empty()) {
            MessageBoxW(dialog, L"O título é obrigatório.", L"BiblioLED", MB_OK | MB_ICONWARNING);
            SetFocus(GetDlgItem(dialog, IDC_LOAN_TITLE));
            return TRUE;
        }
        if (type_index == CB_ERR) {
            MessageBoxW(dialog, L"Seleccione o tipo de empréstimo.", L"BiblioLED",
                        MB_OK | MB_ICONWARNING);
            SetFocus(GetDlgItem(dialog, IDC_LOAN_TYPE));
            return TRUE;
        }

        const std::string loan_date = WideToUtf8(date) + " " + WideToUtf8(time);
        if (ParseDateTime(loan_date) == static_cast<std::time_t>(-1)) {
            MessageBoxW(dialog,
                L"Data ou hora inválida. Utilize AAAA-MM-DD e HH:MM.",
                L"BiblioLED", MB_OK | MB_ICONWARNING);
            SetFocus(GetDlgItem(dialog, IDC_LOAN_DATE));
            return TRUE;
        }

        context->record.title = WideToUtf8(title);
        context->record.loan_date = loan_date;
        context->record.type = type_index == 1 ? "audiolivro" : "livro";
        context->accepted = true;
        EndDialog(dialog, IDOK);
        return TRUE;
    }

    if (message == WM_COMMAND && LOWORD(wParam) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

bool ShowLoanDialog(HWND parent, biblioled::LoanRecord* record)
{
    if (!record) return false;

    RecordDialogContext context;
    context.record = *record;

    const INT_PTR result = DialogBoxParamW(
        gInstance, MAKEINTRESOURCEW(IDD_LOAN_DIALOG), parent,
        LoanDialogProc, reinterpret_cast<LPARAM>(&context));

    if (result != IDOK || !context.accepted) return false;
    *record = context.record;
    return true;
}

int SelectedRecordId()
{
    if (!gLoanList) return 0;
    const int selected = ListView_GetNextItem(gLoanList, -1, LVNI_SELECTED);
    if (selected < 0) return 0;

    LVITEMW item = {};
    item.mask = LVIF_PARAM;
    item.iItem = selected;
    if (!ListView_GetItem(gLoanList, &item)) return 0;
    return static_cast<int>(item.lParam);
}

void ShowStoreError(const wchar_t* operation, const std::string& error)
{
    std::wstring message = operation;
    message += L"\n\n";
    message += Utf8ToWide(error);
    MessageBoxW(gMainWindow, message.c_str(), L"BiblioLED", MB_OK | MB_ICONERROR);
}

bool SynchronizeExternalAuditLog(bool show_error)
{
    if (!gSettings.diagnostics_enabled) {
        gStore.SetExternalAuditLogStatus(false, true);
        UpdateWriteAccess(gMainWindow);
        return true;
    }

    // A legacy database has no audit table until the user confirms migration.
    // Its write operations are already blocked by the store.
    if (gStore.MigrationRequired()) {
        gStore.SetExternalAuditLogStatus(true, true);
        UpdateWriteAccess(gMainWindow);
        return true;
    }

    std::vector<biblioled::AuditEvent> events;
    std::string error;
    bool ok = gStore.GetAuditEvents(&events, &error);
    int written = 0;

    if (ok) {
        const std::wstring log_directory =
            JoinPath(gApplicationDirectory, L"logs");
        ok = biblioled::SynchronizeAuditLog(
            log_directory, events, gSettings.audit_log_max_size_bytes,
            &written, &error);
    }

    gStore.SetExternalAuditLogStatus(true, ok);
    UpdateWriteAccess(gMainWindow);

    if (!ok && show_error) {
        ShowStoreError(
            L"Não foi possível sincronizar o registo de auditoria. "
            L"As alterações à base de dados ficam bloqueadas até o log estar disponível.",
            error);
    }
    return ok;
}

bool OfferLegacyMigration(HWND owner)
{
    if (!gStore.MigrationRequired()) return true;

    std::wstring question =
        L"A base de dados usa o esquema legado (versão 0).\n\n"
        L"A migração para a versão 1 acrescenta uma tabela de auditoria interna. "
        L"Os registos de empréstimos não serão convertidos nem eliminados.\n\n"
        L"Antes da migração será criada e validada uma cópia integral da base de dados. "
        L"A migração é transaccional e exige confirmação.\n\n"
        L"Pretende migrar agora?";

    if (MessageBoxW(owner, question.c_str(), L"Migração da base de dados",
                    MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) {
        MessageBoxW(owner,
            L"A base de dados será aberta em modo só de leitura. "
            L"Pode consultar os empréstimos e criar cópias de segurança, "
            L"mas não pode alterar registos até confirmar a migração.",
            L"Modo só de leitura", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    std::string backup_path;
    std::string error;
    if (!gStore.MigrateLegacySchema(&backup_path, &error)) {
        std::wstring message = L"Não foi possível migrar a base de dados.\n\n";
        message += Utf8ToWide(error);
        MessageBoxW(owner, message.c_str(), L"Migração da base de dados",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring message = L"Base de dados migrada para a versão 1.\n\n";
    message += L"Cópia de segurança anterior à migração:\n";
    message += Utf8ToWide(backup_path);
    MessageBoxW(owner, message.c_str(), L"Migração concluída",
                MB_OK | MB_ICONINFORMATION);
    return true;
}


bool SaveColumnLayout()
{
    if (!gLoanList || !gSettingsFileValid) return false;

    int order[kColumnCount] = {0};
    if (!ListView_GetColumnOrderArray(gLoanList, kColumnCount, order)) return false;

    for (int i = 0; i < kColumnCount; ++i) {
        gSettings.column_order[i] = order[i];
        gSettings.column_widths[i] = ListView_GetColumnWidth(gLoanList, i);
    }

    std::string error;
    if (!WriteSettings(gSettingsPath, gSettings, &error)) {
        if (!gColumnSettingsWarningShown) {
            MessageBoxW(gMainWindow, Utf8ToWide(error).c_str(), L"BiblioLED",
                        MB_OK | MB_ICONWARNING);
            gColumnSettingsWarningShown = true;
        }
        return false;
    }
    return true;
}

void ApplyColumnLayout()
{
    if (gLoanList) {
        ListView_SetColumnOrderArray(gLoanList, kColumnCount, gSettings.column_order);
    }
}

void InsertColumn(int index, const wchar_t* title, int width)
{
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<LPWSTR>(title);
    column.cx = gSettings.column_widths[index] > 0
        ? gSettings.column_widths[index] : width;
    column.iSubItem = index;
    ListView_InsertColumn(gLoanList, index, &column);
}

void RefreshList()
{
    if (!gLoanList) return;

    std::vector<biblioled::LoanRecord> records;
    std::string error;
    if (!gStore.GetAll(&records, &error)) {
        ShowStoreError(L"Não foi possível ler os empréstimos.", error);
        return;
    }

    const std::time_t now = std::time(NULL);
    std::vector<VisibleLoan> visible;
    int invalid_dates = 0;

    for (size_t i = 0; i < records.size(); ++i) {
        const std::time_t loan_time = ParseDateTime(records[i].loan_date);
        if (loan_time == static_cast<std::time_t>(-1)) {
            ++invalid_dates;
            continue;
        }

        const std::time_t expiry = AddDays(loan_time, gSettings.loan_duration_days);
        if (expiry == static_cast<std::time_t>(-1)) {
            ++invalid_dates;
            continue;
        }

        // Pre-entered future records are shown. Expired records remain in the
        // full history and are omitted from this operational view.
        if (expiry >= now) {
            VisibleLoan item;
            item.record = records[i];
            item.expiry = expiry;
            visible.push_back(item);
        }
    }

    std::sort(visible.begin(), visible.end(),
        [](const VisibleLoan& left, const VisibleLoan& right) {
            if (left.expiry != right.expiry) return left.expiry < right.expiry;
            return left.record.id < right.record.id;
        });

    ListView_DeleteAllItems(gLoanList);

    for (size_t i = 0; i < visible.size(); ++i) {
        const biblioled::LoanRecord& record = visible[i].record;
        const std::wstring title = Utf8ToWide(record.title);
        const std::wstring type = record.type == "audiolivro" ? L"Audiolivro" : L"Livro";
        const std::time_t loan_time = ParseDateTime(record.loan_date);
        const std::wstring loan_date = Utf8ToWide(
            FormatDateTime(loan_time, "%d/%m/%Y %H:%M"));
        const std::wstring expiry = Utf8ToWide(
            FormatDateTime(visible[i].expiry, "%d/%m/%Y %H:%M"));
        const long days_left = static_cast<long>(
            std::difftime(visible[i].expiry, now) / 86400.0);
        wchar_t days[32] = {0};
        _snwprintf(days, 31, L"%ld", days_left);

        LVITEMW row = {};
        row.mask = LVIF_TEXT | LVIF_PARAM;
        row.iItem = static_cast<int>(i);
        row.iSubItem = 0;
        row.pszText = const_cast<LPWSTR>(title.c_str());
        row.lParam = record.id;
        const int inserted = ListView_InsertItem(gLoanList, &row);

        if (inserted >= 0) {
            ListView_SetItemText(gLoanList, inserted, 1, const_cast<LPWSTR>(type.c_str()));
            ListView_SetItemText(gLoanList, inserted, 2, const_cast<LPWSTR>(loan_date.c_str()));
            ListView_SetItemText(gLoanList, inserted, 3, const_cast<LPWSTR>(expiry.c_str()));
            ListView_SetItemText(gLoanList, inserted, 4, days);
        }
    }

    int daily_count = 0;
    int monthly_count = 0;
    const std::string today_start = FormatDateTime(now, "%Y-%m-%d") + " 00:00";
    const std::time_t month_start = AddDays(now, -gSettings.monthly_window_days);
    const std::string monthly_start = FormatDateTime(month_start, "%Y-%m-%d %H:%M");

    if (!gStore.CountSince(today_start, &daily_count, &error) ||
        !gStore.CountSince(monthly_start, &monthly_count, &error)) {
        ShowStoreError(L"Não foi possível calcular a disponibilidade.", error);
        return;
    }

    const int daily_available = (gSettings.daily_limit > daily_count)
        ? gSettings.daily_limit - daily_count : 0;
    const int monthly_available = (gSettings.monthly_limit > monthly_count)
        ? gSettings.monthly_limit - monthly_count : 0;

    wchar_t quota[256] = {0};
    _snwprintf(quota, 255,
        L"Disponibilidade: hoje %d/%d livres  |  últimos %d dias %d/%d livres",
        daily_available, gSettings.daily_limit,
        gSettings.monthly_window_days, monthly_available, gSettings.monthly_limit);
    SetWindowTextW(gQuotaLabel, quota);

    wchar_t status[256] = {0};
    _snwprintf(status, 255,
        L"%u empréstimo(s) apresentados  |  Actualizado às %ls%ls%ls%ls",
        static_cast<unsigned int>(visible.size()),
        Utf8ToWide(FormatDateTime(now, "%H:%M:%S")).c_str(),
        invalid_dates ? L"  |  Existem registos com datas inválidas" : L"",
        gStore.MigrationRequired()
            ? L"  |  Só leitura: migração por confirmar" : L"",
        gStore.ExternalAuditLogBlocked()
            ? L"  |  Só leitura: log indisponível" : L"");
    SetWindowTextW(gStatusLabel, status);
}

void AddLoan()
{
    biblioled::LoanRecord record;
    if (!ShowLoanDialog(gMainWindow, &record)) return;

    std::string error;
    if (!gStore.Insert(&record, &error)) {
        ShowStoreError(L"Não foi possível criar o empréstimo.", error);
        return;
    }
    SynchronizeExternalAuditLog(true);
    RefreshList();
}

void CreateManualDatabaseBackup()
{
    std::string backup_path;
    std::string error;
    if (!gStore.CreateManualBackup(&backup_path, &error)) {
        ShowStoreError(L"Não foi possível criar a cópia de segurança.", error);
        return;
    }

    std::wstring message = L"Cópia de segurança criada e validada:\n\n";
    message += Utf8ToWide(backup_path);
    MessageBoxW(gMainWindow, message.c_str(), L"BiblioLED",
                MB_OK | MB_ICONINFORMATION);
}

void RestoreDatabaseFromBackup()
{
    wchar_t filename[MAX_PATH] = {0};
    const std::wstring backup_directory =
        JoinPath(gApplicationDirectory, L"data\\backups");

    static const wchar_t filter[] =
        L"Base de dados SQLite (*.sqlite)\0*.sqlite\0"
        L"Todos os ficheiros (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gMainWindow;
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrInitialDir = backup_directory.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&dialog)) return;

    const std::wstring selected = filename;
    std::wstring confirmation =
        L"A reposição irá substituir todos os registos pelos dados da cópia seleccionada.\n\n";
    confirmation += L"Antes de continuar, será criada e validada uma cópia da base de dados actual.\n\n";
    confirmation += selected;
    confirmation += L"\n\nPretende continuar?";

    if (MessageBoxW(gMainWindow, confirmation.c_str(), L"Confirmar reposição",
                    MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) {
        return;
    }

    std::string safety_backup;
    std::string error;
    if (!gStore.RestoreFromBackup(WideToUtf8(selected), &safety_backup, &error)) {
        ShowStoreError(L"Não foi possível repor a cópia de segurança.", error);
        return;
    }

    std::wstring message = L"Base de dados reposta. A versão anterior foi salvaguardada em:\n\n";
    message += Utf8ToWide(safety_backup);
    MessageBoxW(gMainWindow, message.c_str(), L"BiblioLED",
                MB_OK | MB_ICONINFORMATION);

    OfferLegacyMigration(gMainWindow);
    SynchronizeExternalAuditLog(true);
    UpdateWriteAccess(gMainWindow);
    RefreshList();
}

void EditLoan()
{
    const int id = SelectedRecordId();
    if (id <= 0) {
        MessageBoxW(gMainWindow, L"Seleccione um empréstimo.", L"BiblioLED",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    biblioled::LoanRecord record;
    std::string error;
    if (!gStore.GetById(id, &record, &error)) {
        ShowStoreError(L"Não foi possível abrir o empréstimo.", error);
        return;
    }

    if (!ShowLoanDialog(gMainWindow, &record)) return;
    if (!gStore.Update(record, &error)) {
        ShowStoreError(L"Não foi possível guardar as alterações.", error);
        return;
    }
    SynchronizeExternalAuditLog(true);
    RefreshList();
}

void DeleteLoan()
{
    const int id = SelectedRecordId();
    if (id <= 0) {
        MessageBoxW(gMainWindow, L"Seleccione um empréstimo.", L"BiblioLED",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxW(gMainWindow, L"Eliminar o empréstimo seleccionado?",
                    L"Confirmar eliminação", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    std::string error;
    if (!gStore.Remove(id, &error)) {
        ShowStoreError(L"Não foi possível eliminar o empréstimo.", error);
        return;
    }
    SynchronizeExternalAuditLog(true);
    RefreshList();
}

HMENU CreateApplicationMenu()
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU record_menu = CreatePopupMenu();
    HMENU view_menu = CreatePopupMenu();
    HMENU help_menu = CreatePopupMenu();

    const UINT write_menu_flags = gStore.MigrationRequired() ? MF_GRAYED : MF_STRING;
    AppendMenuW(file_menu, write_menu_flags, IDM_NEW, L"Novo registo\tCtrl+N");
    AppendMenuW(file_menu, MF_STRING, IDM_BACKUP, L"Criar cópia de segurança...");
    AppendMenuW(file_menu, MF_STRING, IDM_RESTORE, L"Repor a partir de cópia...");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file_menu, MF_STRING, IDM_EXIT, L"Sair");

    AppendMenuW(record_menu, write_menu_flags, IDM_EDIT, L"Editar\tEnter");
    AppendMenuW(record_menu, write_menu_flags, IDM_DELETE, L"Eliminar\tDelete");

    AppendMenuW(view_menu, MF_STRING, IDM_REFRESH, L"Actualizar\tF5");
    AppendMenuW(help_menu, MF_STRING, IDM_ABOUT, L"Sobre");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"Ficheiro");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(record_menu), L"Registo");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), L"Ver");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"Ajuda");
    return menu;
}

void CreateChildControls(HWND window)
{
    const DWORD write_button_style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON |
        (gStore.MigrationRequired() ? WS_DISABLED : 0);
    CreateWindowW(L"BUTTON", L"Novo", write_button_style,
        8, 6, 62, 24, window, reinterpret_cast<HMENU>(IDC_BUTTON_NEW), gInstance, NULL);
    CreateWindowW(L"BUTTON", L"Editar", write_button_style,
        74, 6, 62, 24, window, reinterpret_cast<HMENU>(IDC_BUTTON_EDIT), gInstance, NULL);
    CreateWindowW(L"BUTTON", L"Eliminar", write_button_style,
        140, 6, 66, 24, window, reinterpret_cast<HMENU>(IDC_BUTTON_DELETE), gInstance, NULL);
    CreateWindowW(L"BUTTON", L"Actualizar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 6, 78, 24, window, reinterpret_cast<HMENU>(IDC_BUTTON_REFRESH), gInstance, NULL);

    gQuotaLabel = CreateWindowW(L"STATIC", L"Disponibilidade",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        9, 38, 600, 20, window,
        reinterpret_cast<HMENU>(IDC_QUOTA_LABEL), gInstance, NULL);

    gLoanList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        8, 62, 700, 400, window,
        reinterpret_cast<HMENU>(IDC_LOAN_LIST), gInstance, NULL);

    ListView_SetExtendedListViewStyle(gLoanList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER |
        LVS_EX_HEADERDRAGDROP);

    InsertColumn(0, L"Título", 320);
    InsertColumn(1, L"Tipo", 100);
    InsertColumn(2, L"Empréstimo", 130);
    InsertColumn(3, L"Expira em", 130);
    InsertColumn(4, L"Dias", 55);
    ApplyColumnLayout();

    gStatusLabel = CreateWindowW(L"STATIC", L"Pronto.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, 0, 700, 20, window,
        reinterpret_cast<HMENU>(IDC_STATUS_LABEL), gInstance, NULL);
}

void UpdateWriteAccess(HWND window)
{
    if (!window) return;
    const BOOL enabled =
        (gStore.MigrationRequired() || gStore.ExternalAuditLogBlocked()) ? FALSE : TRUE;

    EnableWindow(GetDlgItem(window, IDC_BUTTON_NEW), enabled);
    EnableWindow(GetDlgItem(window, IDC_BUTTON_EDIT), enabled);
    EnableWindow(GetDlgItem(window, IDC_BUTTON_DELETE), enabled);

    HMENU menu = GetMenu(window);
    if (menu) {
        HMENU file_menu = GetSubMenu(menu, 0);
        HMENU record_menu = GetSubMenu(menu, 1);
        const UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED);
        if (file_menu) EnableMenuItem(file_menu, IDM_NEW, flags);
        if (record_menu) {
            EnableMenuItem(record_menu, IDM_EDIT, flags);
            EnableMenuItem(record_menu, IDM_DELETE, flags);
        }
        DrawMenuBar(window);
    }
}

void LayoutControls(HWND window)
{
    RECT client = {};
    GetClientRect(window, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int list_height = height - kToolbarHeight - kQuotaHeight - kStatusHeight - 12;

    if (gQuotaLabel) {
        MoveWindow(gQuotaLabel, 9, kToolbarHeight + 3, width - 18, 18, TRUE);
    }
    if (gLoanList) {
        MoveWindow(gLoanList, 8, kToolbarHeight + kQuotaHeight,
                   width - 16, list_height, TRUE);

    }
    if (gStatusLabel) {
        MoveWindow(gStatusLabel, 8, height - kStatusHeight,
                   width - 16, kStatusHeight - 2, TRUE);
    }
}

void ShowAbout()
{
    std::wstring text = L"BiblioLED\nVersão ";
    text += Utf8ToWide(BIBLIOLED_VERSION);
    text += L"\nGestor pessoal de empréstimos";
    MessageBoxW(gMainWindow, text.c_str(), L"Sobre", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateChildControls(window);
        UpdateWriteAccess(window);
        LayoutControls(window);
        RefreshList();
        return 0;

    case WM_SIZE:
        LayoutControls(window);
        return 0;

    case kPersistColumnsMessage:
        SaveColumnLayout();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_NEW:
        case IDC_BUTTON_NEW:
            AddLoan();
            return 0;
        case IDM_EDIT:
        case IDC_BUTTON_EDIT:
            EditLoan();
            return 0;
        case IDM_DELETE:
        case IDC_BUTTON_DELETE:
            DeleteLoan();
            return 0;
        case IDM_REFRESH:
        case IDC_BUTTON_REFRESH:
            RefreshList();
            return 0;
        case IDM_EXIT:
            DestroyWindow(window);
            return 0;
        case IDM_BACKUP:
            CreateManualDatabaseBackup();
            return 0;
        case IDM_RESTORE:
            RestoreDatabaseFromBackup();
            return 0;
        case IDM_ABOUT:
            ShowAbout();
            return 0;
        }
        break;

    case WM_NOTIFY:
        if (gLoanList &&
            reinterpret_cast<NMHDR*>(lParam)->hwndFrom == ListView_GetHeader(gLoanList)) {
            const UINT code = reinterpret_cast<NMHDR*>(lParam)->code;
            if (code == HDN_ENDDRAGW || code == HDN_ENDTRACKW) {
                PostMessageW(window, kPersistColumnsMessage, 0, 0);
                return FALSE;
            }
        }
        if (reinterpret_cast<NMHDR*>(lParam)->idFrom == IDC_LOAN_LIST &&
            reinterpret_cast<NMHDR*>(lParam)->code == LVN_ITEMACTIVATE) {
            EditLoan();
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_F5) {
            RefreshList();
            return 0;
        }
        if (wParam == VK_DELETE) {
            DeleteLoan();
            return 0;
        }
        break;

    case kActivateMessage:
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
        return 0;

    case WM_DESTROY:
        SaveColumnLayout();
        gStore.Close();
        if (gInstanceMutex) {
            ReleaseMutex(gInstanceMutex);
            CloseHandle(gInstanceMutex);
            gInstanceMutex = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

unsigned long DirectoryHash(const std::wstring& value)
{
    unsigned long hash = 2166136261UL;
    for (size_t i = 0; i < value.size(); ++i) {
        hash ^= static_cast<unsigned long>(value[i]);
        hash *= 16777619UL;
    }
    return hash;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command)
{
    gInstance = instance;
    gApplicationDirectory = GetApplicationDirectory();

    wchar_t mutex_name[80] = {0};
    _snwprintf(mutex_name, 79, L"Local\\BiblioLED_%08lX",
               DirectoryHash(gApplicationDirectory));

    gInstanceMutex = CreateMutexW(NULL, TRUE, mutex_name);
    if (gInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = NULL;
        for (int attempt = 0; attempt < 20 && !existing; ++attempt) {
            existing = FindWindowW(kWindowClass, NULL);
            if (!existing) Sleep(50);
        }
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(gInstanceMutex);
        gInstanceMutex = NULL;
        return 0;
    }

    EnsureDirectory(JoinPath(gApplicationDirectory, L"config"));
    EnsureDirectory(JoinPath(gApplicationDirectory, L"data"));
    EnsureDirectory(JoinPath(gApplicationDirectory, L"data\\backups"));
    EnsureDirectory(JoinPath(gApplicationDirectory, L"exports"));
    EnsureDirectory(JoinPath(gApplicationDirectory, L"logs"));

    std::string settings_error;
    gSettingsPath = JoinPath(gApplicationDirectory, L"config\\settings.ini");
    gSettingsFileValid = LoadSettings(gSettingsPath, &gSettings, &settings_error);
    if (!gSettingsFileValid && !settings_error.empty()) {
        MessageBoxW(NULL, Utf8ToWide(settings_error).c_str(), L"BiblioLED",
                    MB_OK | MB_ICONWARNING);
    }

    const std::wstring database_path =
        JoinPath(gApplicationDirectory, L"data\\biblioteca.sqlite");
    const std::wstring database_marker =
        JoinPath(gApplicationDirectory, L"config\\database.initialized");
    const bool database_exists = FileExistsW(database_path);
    const bool database_marker_exists = FileExistsW(database_marker);

    if (!database_exists && database_marker_exists) {
        std::wstring message =
            L"A base de dados esperada não foi encontrada:\n\n";
        message += database_path;
        message +=
            L"\n\nPor segurança, não foi criada uma base de dados vazia. "
            L"Reponha a base de dados ou uma cópia de segurança e tente novamente.";
        MessageBoxW(NULL, message.c_str(), L"BiblioLED",
                    MB_OK | MB_ICONERROR);
        if (gInstanceMutex) CloseHandle(gInstanceMutex);
        return 1;
    }

    const std::string database_utf8 = WideToUtf8(database_path);
    std::string database_error;
    const std::wstring backup_directory =
        JoinPath(gApplicationDirectory, L"data\\backups");
    const std::string backup_directory_utf8 = WideToUtf8(backup_directory);
    if (!gStore.Open(database_utf8, backup_directory_utf8, &database_error)) {
        MessageBoxW(NULL,
            (L"Não foi possível abrir a base de dados.\n\n" +
             Utf8ToWide(database_error)).c_str(),
            L"BiblioLED", MB_OK | MB_ICONERROR);
        if (gInstanceMutex) CloseHandle(gInstanceMutex);
        return 1;
    }

    if (!database_marker_exists) {
        std::wstring marker_error;
        if (!CreateDatabaseMarker(database_marker, &marker_error)) {
            MessageBoxW(NULL, marker_error.c_str(), L"BiblioLED",
                        MB_OK | MB_ICONERROR);
            gStore.Close();
            if (gInstanceMutex) CloseHandle(gInstanceMutex);
            return 1;
        }
    }

    OfferLegacyMigration(NULL);
    SynchronizeExternalAuditLog(true);

    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    gApplicationIcon = LoadIconW(gInstance, MAKEINTRESOURCEW(IDI_BIBLIOLED));

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = MainWindowProc;
    window_class.hInstance = gInstance;
    window_class.hIcon = gApplicationIcon ? gApplicationIcon : LoadIconW(NULL, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    window_class.lpszClassName = kWindowClass;
    window_class.hIconSm = window_class.hIcon;

    if (!RegisterClassExW(&window_class)) {
        MessageBoxW(NULL, L"Não foi possível registar a janela principal.",
                    L"BiblioLED", MB_OK | MB_ICONERROR);
        gStore.Close();
        if (gInstanceMutex) CloseHandle(gInstanceMutex);
        return 1;
    }

    std::wstring title = L"BiblioLED v";
    title += Utf8ToWide(BIBLIOLED_VERSION);

    gMainWindow = CreateWindowExW(
        0, kWindowClass, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        NULL, CreateApplicationMenu(), gInstance, NULL);

    if (!gMainWindow) {
        MessageBoxW(NULL, L"Não foi possível criar a janela principal.",
                    L"BiblioLED", MB_OK | MB_ICONERROR);
        gStore.Close();
        if (gInstanceMutex) CloseHandle(gInstanceMutex);
        return 1;
    }

    if (gApplicationIcon) {
        SendMessageW(gMainWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(gApplicationIcon));
        SendMessageW(gMainWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(gApplicationIcon));
    }

    ShowWindow(gMainWindow, show_command);
    UpdateWindow(gMainWindow);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
