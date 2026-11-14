#include "history_dialog.h"
#include "biblioled/loan_store.h"
#include "resource.h"
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace {
struct HistoryRow {
    int id;
    std::wstring id_text, title, type, type_key, date, search_key;
    HistoryRow() : id(0) {}
};
struct HistoryContext {
    HWND owner;
    biblioled::LoanStore* store;
    std::vector<HistoryRow> rows;
    unsigned int invalid_dates;
    HistoryContext() : owner(NULL), store(NULL), invalid_dates(0) {}
};

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
std::wstring Lowercase(std::wstring value)
{
    if (!value.empty()) CharLowerBuffW(&value[0], static_cast<DWORD>(value.size()));
    return value;
}
bool HasDateTimeShape(const std::string& value)
{
    if (value.size() < 16 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':') return false;
    for (size_t i=0; i<16; ++i) {
        if (i==4 || i==7 || i==10 || i==13) continue;
        if (value[i]<'0' || value[i]>'9') return false;
    }
    return true;
}
std::wstring DisplayDate(const std::string& value)
{
    if (HasDateTimeShape(value)) {
        return Utf8ToWide(value.substr(8,2)+"/"+value.substr(5,2)+"/"+
                          value.substr(0,4)+value.substr(10));
    }
    return Utf8ToWide(value);
}
bool LoadRows(HistoryContext* context, std::string* error)
{
    if (!context || !context->store) {
        if (error) *error = "Invalid history context.";
        return false;
    }
    std::vector<biblioled::LoanRecord> records;
    if (!context->store->GetAll(&records,error)) return false;
    context->rows.clear();
    context->rows.reserve(records.size());
    context->invalid_dates=0;
    for (size_t i=0;i<records.size();++i) {
        const biblioled::LoanRecord& record=records[i];
        HistoryRow row;
        row.id=record.id;
        wchar_t id[24]={0};
        _snwprintf(id,23,L"%d",record.id);
        row.id_text=id;
        row.title=Utf8ToWide(record.title);
        row.type_key=Lowercase(Utf8ToWide(record.type));
        if (row.type_key==L"audiolivro") row.type=L"Audiolivro";
        else if (row.type_key==L"livro") row.type=L"Livro";
        else row.type=Utf8ToWide(record.type);
        row.date=DisplayDate(record.loan_date);
        if (!HasDateTimeShape(record.loan_date)) ++context->invalid_dates;
        row.search_key=Lowercase(row.title+L" "+row.type+L" "+row.date+L" "+row.id_text);
        context->rows.push_back(row);
    }
    return true;
}
std::wstring ControlText(HWND dialog,int id)
{
    HWND control=GetDlgItem(dialog,id);
    const int length=control?GetWindowTextLengthW(control):0;
    std::wstring value(static_cast<size_t>(length)+1,L'\0');
    if (control && length>0) GetWindowTextW(control,&value[0],length+1);
    value.resize(static_cast<size_t>(length));
    return value;
}
void UpdateActions(HWND dialog,const HistoryContext* context)
{
    const BOOL enabled=context && context->store &&
        !context->store->MigrationRequired() &&
        !context->store->ExternalAuditLogBlocked();
    EnableWindow(GetDlgItem(dialog,IDC_HISTORY_EDIT),enabled);
    EnableWindow(GetDlgItem(dialog,IDC_HISTORY_DELETE),enabled);
}
void RefreshList(HWND dialog,const HistoryContext* context)
{
    if (!dialog || !context) return;
    const std::wstring query=Lowercase(ControlText(dialog,IDC_HISTORY_SEARCH));
    const LRESULT kind=SendDlgItemMessageW(dialog,IDC_HISTORY_TYPE,CB_GETCURSEL,0,0);
    HWND list=GetDlgItem(dialog,IDC_HISTORY_LIST);
    ListView_DeleteAllItems(list);
    unsigned int shown=0;
    for(size_t i=0;i<context->rows.size();++i) {
        const HistoryRow& row=context->rows[i];
        if(kind==1 && row.type_key!=L"livro") continue;
        if(kind==2 && row.type_key!=L"audiolivro") continue;
        if(!query.empty() && row.search_key.find(query)==std::wstring::npos) continue;
        LVITEMW item={};
        item.mask=LVIF_TEXT|LVIF_PARAM;
        item.iItem=static_cast<int>(shown);
        item.pszText=const_cast<LPWSTR>(row.id_text.c_str());
        item.lParam=static_cast<LPARAM>(row.id);
        const int inserted=ListView_InsertItem(list,&item);
        if(inserted>=0) {
            ListView_SetItemText(list,inserted,1,const_cast<LPWSTR>(row.title.c_str()));
            ListView_SetItemText(list,inserted,2,const_cast<LPWSTR>(row.type.c_str()));
            ListView_SetItemText(list,inserted,3,const_cast<LPWSTR>(row.date.c_str()));
            ++shown;
        }
    }
    wchar_t status[256]={0};
    _snwprintf(status,255,L"%u de %u registo(s)",shown,
               static_cast<unsigned int>(context->rows.size()));
    if(context->invalid_dates>0) {
        const size_t used=wcslen(status);
        _snwprintf(status+used,255-used,L"  |  %u data(s) não reconhecida(s)",
                   context->invalid_dates);
    }
    SetDlgItemTextW(dialog,IDC_HISTORY_STATUS,status);
    UpdateActions(dialog,context);
}
int SelectedId(HWND dialog)
{
    HWND list=GetDlgItem(dialog,IDC_HISTORY_LIST);
    const int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED);
    if(selected<0) return 0;
    LVITEMW item={};
    item.mask=LVIF_PARAM;
    item.iItem=selected;
    return ListView_GetItem(list,&item)?static_cast<int>(item.lParam):0;
}
void Reload(HWND dialog,HistoryContext* context)
{
    std::string error;
    if(!LoadRows(context,&error)) {
        MessageBoxW(dialog,(L"Não foi possível actualizar o histórico.\n\n"+
            Utf8ToWide(error)).c_str(),L"BiblioLED",MB_OK|MB_ICONERROR);
        return;
    }
    RefreshList(dialog,context);
}
void EditSelected(HWND dialog,HistoryContext* context)
{
    const int id=SelectedId(dialog);
    if(id<=0) {
        MessageBoxW(dialog,L"Seleccione um registo do histórico.",
                    L"Histórico",MB_OK|MB_ICONINFORMATION);
        return;
    }
    if(SendMessageW(context->owner,biblioled::win32::kHistoryEditMessage,
                    static_cast<WPARAM>(id),reinterpret_cast<LPARAM>(dialog)))
        Reload(dialog,context);
    UpdateActions(dialog,context);
}
void DeleteSelected(HWND dialog,HistoryContext* context)
{
    const int id=SelectedId(dialog);
    if(id<=0) {
        MessageBoxW(dialog,L"Seleccione um registo do histórico.",
                    L"Histórico",MB_OK|MB_ICONINFORMATION);
        return;
    }
    if(MessageBoxW(dialog,L"Eliminar o registo seleccionado?",
        L"Confirmar eliminação",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES) return;
    if(SendMessageW(context->owner,biblioled::win32::kHistoryDeleteMessage,
                    static_cast<WPARAM>(id),reinterpret_cast<LPARAM>(dialog)))
        Reload(dialog,context);
    UpdateActions(dialog,context);
}
INT_PTR CALLBACK HistoryProc(HWND dialog,UINT message,WPARAM wParam,LPARAM lParam)
{
    HistoryContext* context=reinterpret_cast<HistoryContext*>(
        GetWindowLongPtrW(dialog,DWLP_USER));
    if(message==WM_INITDIALOG) {
        context=reinterpret_cast<HistoryContext*>(lParam);
        SetWindowLongPtrW(dialog,DWLP_USER,reinterpret_cast<LONG_PTR>(context));
        SendDlgItemMessageW(dialog,IDC_HISTORY_TYPE,CB_ADDSTRING,0,
            reinterpret_cast<LPARAM>(L"Todos os tipos"));
        SendDlgItemMessageW(dialog,IDC_HISTORY_TYPE,CB_ADDSTRING,0,
            reinterpret_cast<LPARAM>(L"Livro"));
        SendDlgItemMessageW(dialog,IDC_HISTORY_TYPE,CB_ADDSTRING,0,
            reinterpret_cast<LPARAM>(L"Audiolivro"));
        SendDlgItemMessageW(dialog,IDC_HISTORY_TYPE,CB_SETCURSEL,0,0);
        HWND list=GetDlgItem(dialog,IDC_HISTORY_LIST);
        ListView_SetExtendedListViewStyle(list,
            LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        const wchar_t* labels[]={L"ID",L"Título",L"Tipo",L"Data e hora"};
        const int widths[]={48,245,88,130};
        for(int i=0;i<4;++i) {
            LVCOLUMNW column={};
            column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
            column.pszText=const_cast<LPWSTR>(labels[i]);
            column.cx=widths[i];
            column.iSubItem=i;
            ListView_InsertColumn(list,i,&column);
        }
        std::string error;
        if(!LoadRows(context,&error)) {
            MessageBoxW(dialog,(L"Não foi possível ler o histórico.\n\n"+
                Utf8ToWide(error)).c_str(),L"BiblioLED",MB_OK|MB_ICONERROR);
            EndDialog(dialog,IDCANCEL);
            return TRUE;
        }
        RefreshList(dialog,context);
        SetFocus(GetDlgItem(dialog,IDC_HISTORY_SEARCH));
        return FALSE;
    }
    if(message==WM_COMMAND && context) {
        const int id=LOWORD(wParam), notification=HIWORD(wParam);
        if(id==IDC_HISTORY_SEARCH && notification==EN_CHANGE) {
            RefreshList(dialog,context); return TRUE;
        }
        if(id==IDC_HISTORY_TYPE && notification==CBN_SELCHANGE) {
            RefreshList(dialog,context); return TRUE;
        }
        if(id==IDC_HISTORY_EDIT) { EditSelected(dialog,context); return TRUE; }
        if(id==IDC_HISTORY_DELETE) { DeleteSelected(dialog,context); return TRUE; }
        if(id==IDOK || id==IDCANCEL) { EndDialog(dialog,id); return TRUE; }
    }
    if(message==WM_NOTIFY && context) {
        const NMHDR* header=reinterpret_cast<const NMHDR*>(lParam);
        if(header && header->idFrom==IDC_HISTORY_LIST) {
            if(header->code==LVN_ITEMACTIVATE) {
                EditSelected(dialog,context); return TRUE;
            }
            if(header->code==LVN_KEYDOWN) {
                const NMLVKEYDOWN* key=reinterpret_cast<const NMLVKEYDOWN*>(lParam);
                if(key->wVKey==VK_RETURN) { EditSelected(dialog,context); return TRUE; }
                if(key->wVKey==VK_DELETE) { DeleteSelected(dialog,context); return TRUE; }
                if(key->wVKey==VK_F5) { Reload(dialog,context); return TRUE; }
            }
        }
    }
    return FALSE;
}
} // namespace

namespace biblioled {
namespace win32 {
void ShowHistoryDialog(HINSTANCE instance,HWND owner,LoanStore* store)
{
    if(!store) return;
    HistoryContext context;
    context.owner=owner;
    context.store=store;
    DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_HISTORY_DIALOG),owner,
                    HistoryProc,reinterpret_cast<LPARAM>(&context));
}
}
}
