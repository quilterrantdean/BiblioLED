#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "config_store.h"
#include "loans_repo.h"
#include "metrics.h"
#include "csv_export.h"

#define ID_BTN_REFRESH 1001
#define ID_BTN_NEW     1002
#define ID_BTN_HISTORY 1003
#define ID_BTN_EXPORT  1004
#define ID_BTN_STATS   1005
#define ID_LBL_STATUS  2001
#define ID_LIST_EXP    3001

#define ID_DLG_TITLE   4001
#define ID_DLG_DATE    4002
#define ID_DLG_TYPE    4003
#define ID_DLG_OK      4004
#define ID_DLG_CANCEL  4005

#define ID_HIST_LIST   5001
#define ID_HIST_EDIT   5002
#define ID_HIST_DEL    5003
#define ID_HIST_SEARCH 5004
#define ID_HIST_FILTER 5005

typedef struct { app_config_t cfg; HWND h_status; HWND h_list; } app_ctx_t;
typedef struct { char title[256]; char date[20]; char type[32]; int accepted; } record_data_t;
typedef struct { app_ctx_t *ctx; HWND h_list; HWND h_filter; loan_record_t rows[2048]; int count; } history_ctx_t;

static void now_str(char out[20]) { time_t t=time(NULL); struct tm *lt=localtime(&t); strftime(out,20,"%Y-%m-%d %H:%M",lt); }
static void set_status(HWND h, const char *msg) { SetWindowTextA(h, msg ? msg : ""); }
static int validate_datetime_str(const char *s) { int y,m,d,hh,mm; return sscanf(s, "%d-%d-%d %d:%d", &y,&m,&d,&hh,&mm)==5; }

static void show_stats(HWND parent, app_ctx_t *ctx) {
    loan_record_t rows[4096]; int n=0, i, livros=0, audios=0;
    char msg[1024];
    loans_read_all(&ctx->cfg, rows, 4096, &n, NULL, 0);
    for (i=0; i<n && i<4096; i++) {
        if (_stricmp(rows[i].type, "livro")==0) livros++;
        else if (_stricmp(rows[i].type, "audiolivro")==0) audios++;
    }
    {
        char first_date[20] = "-";
        char last_date[20] = "-";
        if (n > 0) {
            snprintf(last_date, sizeof(last_date), "%s", rows[0].loan_date);
            snprintf(first_date, sizeof(first_date), "%s", rows[n-1].loan_date);
        }
        snprintf(msg, sizeof(msg),
            "Total de registos: %d\nLivros: %d\nAudiolivros: %d\nPrimeiro registo: %s\nÚltimo registo: %s",
            n, livros, audios, first_date, last_date);
    }
    MessageBoxA(parent, msg, "Estatísticas", MB_OK|MB_ICONINFORMATION);
}

static LRESULT CALLBACK record_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    record_data_t *data = (record_data_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        data = (record_data_t*)cs->lpCreateParams;
        CreateWindowA("STATIC", "Título:", WS_CHILD|WS_VISIBLE, 12,14,60,20, hwnd,NULL,NULL,NULL);
        CreateWindowA("EDIT", data->title, WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 80,12,300,22, hwnd,(HMENU)ID_DLG_TITLE,NULL,NULL);
        CreateWindowA("STATIC", "Data/Hora:", WS_CHILD|WS_VISIBLE, 12,46,70,20, hwnd,NULL,NULL,NULL);
        CreateWindowA("EDIT", data->date, WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 80,44,160,22, hwnd,(HMENU)ID_DLG_DATE,NULL,NULL);
        CreateWindowA("STATIC", "Tipo:", WS_CHILD|WS_VISIBLE, 250,46,35,20, hwnd,NULL,NULL,NULL);
        CreateWindowA("EDIT", data->type, WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 290,44,90,22, hwnd,(HMENU)ID_DLG_TYPE,NULL,NULL);
        CreateWindowA("BUTTON", "OK", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, 210,80,80,28, hwnd,(HMENU)ID_DLG_OK,NULL,NULL);
        CreateWindowA("BUTTON", "Cancelar", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 300,80,80,28, hwnd,(HMENU)ID_DLG_CANCEL,NULL,NULL);
        return 0;
    }
    case WM_COMMAND:
        if (!data) return 0;
        if (LOWORD(wParam)==ID_DLG_OK) {
            GetWindowTextA(GetDlgItem(hwnd, ID_DLG_TITLE), data->title, sizeof(data->title));
            GetWindowTextA(GetDlgItem(hwnd, ID_DLG_DATE), data->date, sizeof(data->date));
            GetWindowTextA(GetDlgItem(hwnd, ID_DLG_TYPE), data->type, sizeof(data->type));
            if (!data->title[0]) { MessageBoxA(hwnd,"Título obrigatório.","Validação",MB_OK|MB_ICONWARNING); return 0; }
            if (!validate_datetime_str(data->date)) { MessageBoxA(hwnd,"Data inválida. Use YYYY-MM-DD HH:MM", "Validação", MB_OK|MB_ICONWARNING); return 0; }
            if (!data->type[0]) strcpy(data->type, "livro");
            data->accepted=1; DestroyWindow(hwnd); return 0;
        }
        if (LOWORD(wParam)==ID_DLG_CANCEL) { data->accepted=0; DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE: if (data) data->accepted=0; DestroyWindow(hwnd); return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

static int show_record_dialog(HWND parent, record_data_t *out, const char *title) {
    WNDCLASSA wc; HWND dlg; MSG msg;
    memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=record_proc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName="BiblioRecordDlg"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    RegisterClassA(&wc);
    dlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "BiblioRecordDlg", title ? title : "Registo", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,410,150, parent,NULL,GetModuleHandle(NULL), out);
    if(!dlg) return 1;
    ShowWindow(dlg,SW_SHOW); EnableWindow(parent,FALSE);
    while(IsWindow(dlg) && GetMessage(&msg,NULL,0,0)){ if(!IsDialogMessage(dlg,&msg)){ TranslateMessage(&msg); DispatchMessage(&msg);} }
    EnableWindow(parent,TRUE); SetActiveWindow(parent); return 0;
}

static void refresh_dashboard(app_ctx_t *ctx) {
    dashboard_metrics_t m; char line[512]; int i;
    if (metrics_get_dashboard(&ctx->cfg,&m,NULL,0)!=0) { set_status(ctx->h_status,"Erro ao calcular métricas."); return; }
    ListView_DeleteAllItems(ctx->h_list);
    for(i=0;i<m.expiries_count;++i){ LVITEMA it; char days[32]; ZeroMemory(&it,sizeof(it)); it.mask=LVIF_TEXT; it.iItem=i; it.pszText=m.expiries[i].title; ListView_InsertItem(ctx->h_list,&it); ListView_SetItemText(ctx->h_list,i,1,m.expiries[i].expiry_date); sprintf(days,"%d",m.expiries[i].days_left); ListView_SetItemText(ctx->h_list,i,2,days); ListView_SetItemText(ctx->h_list,i,3,m.expiries[i].type); }
    sprintf(line,"Disponíveis hoje: %d | Disponíveis mês: %d | Expirações: %d",m.daily_available,m.monthly_available,m.expiries_count);
    set_status(ctx->h_status,line);
}

static void history_reload(history_ctx_t *hctx, const char *filter) {
    int i, shown=0;
    loans_read_all(&hctx->ctx->cfg, hctx->rows, 2048, &hctx->count, NULL, 0);
    ListView_DeleteAllItems(hctx->h_list);
    for (i=0; i<hctx->count && i<2048; i++) {
        LVITEMA it; char idb[16];
        if (filter && filter[0]) {
            char t1[256], t2[256]; int k;
            snprintf(t1, sizeof(t1), "%s", hctx->rows[i].title);
            snprintf(t2, sizeof(t2), "%s", filter);
            for (k=0; t1[k]; ++k) if (t1[k]>='A' && t1[k]<='Z') t1[k] = (char)(t1[k]-'A'+'a');
            for (k=0; t2[k]; ++k) if (t2[k]>='A' && t2[k]<='Z') t2[k] = (char)(t2[k]-'A'+'a');
            if (strstr(t1, t2)==NULL) continue;
        }
        ZeroMemory(&it,sizeof(it)); it.mask=LVIF_TEXT; it.iItem=shown;
        sprintf(idb, "%d", hctx->rows[i].id);
        it.pszText = idb; ListView_InsertItem(hctx->h_list, &it);
        ListView_SetItemText(hctx->h_list, shown, 1, hctx->rows[i].loan_date);
        ListView_SetItemText(hctx->h_list, shown, 2, hctx->rows[i].type);
        ListView_SetItemText(hctx->h_list, shown, 3, hctx->rows[i].title);
        shown++;
    }
}

static int selected_id(history_ctx_t *hctx) {
    int sel = ListView_GetNextItem(hctx->h_list, -1, LVNI_SELECTED);
    char idbuf[16];
    if (sel < 0) return -1;
    ListView_GetItemText(hctx->h_list, sel, 0, idbuf, sizeof(idbuf));
    return atoi(idbuf);
}

static LRESULT CALLBACK history_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    history_ctx_t *hctx = (history_ctx_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA*)lParam;
        RECT rc; LVCOLUMNA col;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        hctx = (history_ctx_t*)cs->lpCreateParams;
        CreateWindowA("BUTTON", "Editar", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 10, 80, 24, hwnd, (HMENU)ID_HIST_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Eliminar", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 100, 10, 80, 24, hwnd, (HMENU)ID_HIST_DEL, NULL, NULL);
        hctx->h_filter = CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 200,10,220,24, hwnd, (HMENU)ID_HIST_FILTER, NULL, NULL);
        CreateWindowA("BUTTON", "Pesquisar", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 430,10,90,24, hwnd, (HMENU)ID_HIST_SEARCH, NULL, NULL);
        GetClientRect(hwnd, &rc);
        hctx->h_list = CreateWindowA(WC_LISTVIEWA, "", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL, 10, 42, rc.right-20, rc.bottom-52, hwnd, (HMENU)ID_HIST_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(hctx->h_list, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
        ZeroMemory(&col,sizeof(col)); col.mask=LVCF_TEXT|LVCF_WIDTH;
        col.pszText="ID"; col.cx=50; ListView_InsertColumn(hctx->h_list,0,&col);
        col.pszText="Data"; col.cx=140; ListView_InsertColumn(hctx->h_list,1,&col);
        col.pszText="Tipo"; col.cx=120; ListView_InsertColumn(hctx->h_list,2,&col);
        col.pszText="Título"; col.cx=380; ListView_InsertColumn(hctx->h_list,3,&col);
        history_reload(hctx, "");
        return 0;
    }
    case WM_SIZE:
        if (hctx && hctx->h_list) {
            RECT rc; GetClientRect(hwnd, &rc);
            MoveWindow(hctx->h_list, 10, 42, rc.right-20, rc.bottom-52, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (!hctx) return 0;
        if (LOWORD(wParam)==ID_HIST_SEARCH) {
            char f[256]; GetWindowTextA(hctx->h_filter, f, sizeof(f)); history_reload(hctx, f); return 0;
        }
        if (LOWORD(wParam)==ID_HIST_DEL) {
            int id = selected_id(hctx);
            if (id < 0) { MessageBoxA(hwnd,"Seleccione um registo.","Histórico",MB_OK|MB_ICONINFORMATION); return 0; }
            if (MessageBoxA(hwnd, "Confirma eliminação?", "Confirmar", MB_YESNO|MB_ICONQUESTION)!=IDYES) return 0;
            loans_delete(&hctx->ctx->cfg, id, NULL, 0);
            history_reload(hctx, "");
            refresh_dashboard(hctx->ctx);
            return 0;
        }
        if (LOWORD(wParam)==ID_HIST_EDIT) {
            int id = selected_id(hctx);
            record_data_t d; int found = -1; int i;
            if (id < 0) { MessageBoxA(hwnd,"Seleccione um registo.","Histórico",MB_OK|MB_ICONINFORMATION); return 0; }
            for (i=0; i<hctx->count; i++) if (hctx->rows[i].id == id) { found = i; break; }
            if (found < 0) return 0;
            memset(&d,0,sizeof(d));
            strcpy(d.title, hctx->rows[found].title);
            strcpy(d.date, hctx->rows[found].loan_date);
            strcpy(d.type, hctx->rows[found].type);
            show_record_dialog(hwnd, &d, "Editar Registo");
            if (d.accepted) {
                loans_edit(&hctx->ctx->cfg, id, d.title, d.date, d.type, NULL, 0);
                history_reload(hctx, "");
                refresh_dashboard(hctx->ctx);
            }
            return 0;
        }
        return 0;
    case WM_CLOSE: if (hctx) free(hctx); DestroyWindow(hwnd); return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

static void open_history_dialog(HWND parent, app_ctx_t *ctx) {
    WNDCLASSA wc; HWND win; history_ctx_t *hctx=(history_ctx_t*)calloc(1,sizeof(history_ctx_t));
    if(!hctx) return;
    hctx->ctx = ctx;
    memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=history_proc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName="BiblioHistoryWin"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassA(&wc);
    win=CreateWindowA("BiblioHistoryWin","Histórico Completo",WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT,CW_USEDEFAULT,760,460, parent,NULL,GetModuleHandle(NULL),hctx);
    if(!win) free(hctx);
}

static void export_csv_gui(HWND parent, app_ctx_t *ctx) {
    loan_record_t rows[4096];
    int n = 0;
    int i;
    char ts[20];
    char out[260];

    now_str(ts);
    for (i = 0; ts[i]; i++) {
        if (ts[i] == ' ' || ts[i] == ':') ts[i] = '_';
    }

    snprintf(out, sizeof(out), "export_%s.csv", ts);

    loans_read_all(&ctx->cfg, rows, 4096, &n, NULL, 0);
    if (csv_export_history(&ctx->cfg, rows, n, out, NULL, 0)==0) {
        char msg[320]; snprintf(msg,sizeof(msg),"Exportação concluída: %s", out);
        MessageBoxA(parent, msg, "Exportar CSV", MB_OK|MB_ICONINFORMATION);
    } else {
        MessageBoxA(parent, "Falha na exportação CSV.", "Erro", MB_OK|MB_ICONERROR);
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    app_ctx_t *ctx=(app_ctx_t*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
    switch(msg){
    case WM_CREATE:{
        LVCOLUMNA col; RECT rc; app_ctx_t *new_ctx=(app_ctx_t*)calloc(1,sizeof(app_ctx_t)); if(!new_ctx) return -1;
        config_load_user_settings("user_settings.json", &new_ctx->cfg, NULL, 0);
        loans_init_db(&new_ctx->cfg,NULL,0);
        SetWindowLongPtr(hwnd,GWLP_USERDATA,(LONG_PTR)new_ctx); ctx=new_ctx;
        CreateWindowA("BUTTON","Actualizar",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,10,10,100,26,hwnd,(HMENU)ID_BTN_REFRESH,GetModuleHandle(NULL),NULL);
        CreateWindowA("BUTTON","Novo Registo",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,120,10,120,26,hwnd,(HMENU)ID_BTN_NEW,GetModuleHandle(NULL),NULL);
        CreateWindowA("BUTTON","Histórico",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,250,10,100,26,hwnd,(HMENU)ID_BTN_HISTORY,GetModuleHandle(NULL),NULL);
        CreateWindowA("BUTTON","Exportar CSV",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,360,10,100,26,hwnd,(HMENU)ID_BTN_EXPORT,GetModuleHandle(NULL),NULL);
        CreateWindowA("BUTTON","Estatísticas",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,470,10,100,26,hwnd,(HMENU)ID_BTN_STATS,GetModuleHandle(NULL),NULL);
        GetClientRect(hwnd,&rc);
        ctx->h_list=CreateWindowA(WC_LISTVIEWA,"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,10,45,rc.right-20,rc.bottom-90,hwnd,(HMENU)ID_LIST_EXP,GetModuleHandle(NULL),NULL);
        ListView_SetExtendedListViewStyle(ctx->h_list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
        ZeroMemory(&col,sizeof(col)); col.mask=LVCF_TEXT|LVCF_WIDTH;
        col.pszText="Título"; col.cx=300; ListView_InsertColumn(ctx->h_list,0,&col);
        col.pszText="Expira em"; col.cx=140; ListView_InsertColumn(ctx->h_list,1,&col);
        col.pszText="Dias"; col.cx=70; ListView_InsertColumn(ctx->h_list,2,&col);
        col.pszText="Tipo"; col.cx=110; ListView_InsertColumn(ctx->h_list,3,&col);
        ctx->h_status=CreateWindowA("STATIC","A carregar...",WS_CHILD|WS_VISIBLE,10,rc.bottom-35,rc.right-20,20,hwnd,(HMENU)ID_LBL_STATUS,GetModuleHandle(NULL),NULL);
        refresh_dashboard(ctx); return 0; }
    case WM_SIZE:
        if(ctx&&ctx->h_list&&ctx->h_status){RECT rc; GetClientRect(hwnd,&rc); MoveWindow(ctx->h_list,10,45,rc.right-20,rc.bottom-90,TRUE); MoveWindow(ctx->h_status,10,rc.bottom-35,rc.right-20,20,TRUE);} return 0;
    case WM_COMMAND:
        if(!ctx) return 0;
        if(LOWORD(wParam)==ID_BTN_REFRESH){ refresh_dashboard(ctx); return 0; }
        if(LOWORD(wParam)==ID_BTN_NEW){ record_data_t data; memset(&data,0,sizeof(data)); strcpy(data.type,"livro"); now_str(data.date); show_record_dialog(hwnd,&data,"Novo Registo"); if(data.accepted){ if(loans_add(&ctx->cfg,data.title,data.date,data.type,NULL,0)!=0) MessageBoxA(hwnd,"Falha ao guardar registo.","Erro",MB_OK|MB_ICONERROR); refresh_dashboard(ctx);} return 0; }
        if(LOWORD(wParam)==ID_BTN_HISTORY){ open_history_dialog(hwnd,ctx); return 0; }
        if(LOWORD(wParam)==ID_BTN_EXPORT){ export_csv_gui(hwnd, ctx); return 0; }
        if(LOWORD(wParam)==ID_BTN_STATS){ show_stats(hwnd, ctx); return 0; }
        return 0;
    case WM_DESTROY: if(ctx) free(ctx); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASSA wc; HWND hwnd; MSG msg; INITCOMMONCONTROLSEX icc; (void)hPrev; (void)lpCmd;
    icc.dwSize=sizeof(icc); icc.dwICC=ICC_LISTVIEW_CLASSES; InitCommonControlsEx(&icc);
    ZeroMemory(&wc,sizeof(wc)); wc.lpfnWndProc=wnd_proc; wc.hInstance=hInst; wc.lpszClassName="BiblioLEDWinXP"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassA(&wc)) return 1;
    hwnd=CreateWindowA("BiblioLEDWinXP","BiblioLED C99 (GUI Win32)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,980,600,NULL,NULL,hInst,NULL);
    if(!hwnd) return 2;
    ShowWindow(hwnd,nShow); UpdateWindow(hwnd);
    while(GetMessage(&msg,NULL,0,0)>0){ TranslateMessage(&msg); DispatchMessage(&msg);} return (int)msg.wParam;
}
