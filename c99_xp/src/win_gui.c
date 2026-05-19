#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "config_store.h"
#include "loans_repo.h"
#include "metrics.h"

#define ID_BTN_REFRESH 1001
#define ID_BTN_NEW     1002
#define ID_BTN_HISTORY 1003
#define ID_LBL_STATUS  2001
#define ID_LIST_EXP    3001

#define ID_DLG_TITLE   4001
#define ID_DLG_DATE    4002
#define ID_DLG_TYPE    4003
#define ID_DLG_OK      4004
#define ID_DLG_CANCEL  4005

typedef struct {
    app_config_t cfg;
    HWND h_status;
    HWND h_list;
} app_ctx_t;

typedef struct {
    char title[256];
    char date[20];
    char type[32];
    int accepted;
} new_record_data_t;

static void now_str(char out[20]) { time_t t=time(NULL); struct tm *lt=localtime(&t); strftime(out,20,"%Y-%m-%d %H:%M",lt); }
static void set_status(HWND h, const char *msg) { SetWindowTextA(h, msg ? msg : ""); }
static int validate_datetime_str(const char *s) { int y,m,d,hh,mm; return sscanf(s, "%d-%d-%d %d:%d", &y,&m,&d,&hh,&mm)==5; }

static LRESULT CALLBACK new_record_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    new_record_data_t *data = (new_record_data_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA *cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        data = (new_record_data_t*)cs->lpCreateParams;
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

static int show_new_record_dialog(HWND parent, new_record_data_t *out) {
    WNDCLASSA wc; HWND dlg; MSG msg;
    memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=new_record_proc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName="BiblioNewRecordDlg"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    RegisterClassA(&wc);
    dlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "BiblioNewRecordDlg", "Novo Registo", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,410,150, parent,NULL,GetModuleHandle(NULL), out);
    if(!dlg) return 1;
    ShowWindow(dlg,SW_SHOW); EnableWindow(parent,FALSE);
    while(IsWindow(dlg) && GetMessage(&msg,NULL,0,0)){ if(!IsDialogMessage(dlg,&msg)){ TranslateMessage(&msg); DispatchMessage(&msg);} }
    EnableWindow(parent,TRUE); SetActiveWindow(parent); return 0;
}

static void refresh_dashboard(app_ctx_t *ctx) {
    dashboard_metrics_t m; char line[512]; int i;
    if (metrics_get_dashboard(&ctx->cfg,&m,NULL,0)!=0) { set_status(ctx->h_status,"Erro ao calcular métricas."); return; }
    ListView_DeleteAllItems(ctx->h_list);
    for(i=0;i<m.expiries_count;++i){ LVITEMA it; char days[32]; ZeroMemory(&it,sizeof(it)); it.mask=LVIF_TEXT; it.iItem=i; it.pszText=m.expiries[i].title; ListView_InsertItemA(ctx->h_list,&it); ListView_SetItemText(ctx->h_list,i,1,m.expiries[i].expiry_date); sprintf(days,"%d",m.expiries[i].days_left); ListView_SetItemText(ctx->h_list,i,2,days); ListView_SetItemText(ctx->h_list,i,3,m.expiries[i].type); }
    sprintf(line,"Disponíveis hoje: %d | Disponíveis mês: %d | Expirações: %d",m.daily_available,m.monthly_available,m.expiries_count);
    set_status(ctx->h_status,line);
}

static void show_history_window(HWND parent, app_ctx_t *ctx) {
    loan_record_t rows[1024]; int n=0, i;
    if (loans_read_all(&ctx->cfg, rows, 1024, &n, NULL, 0) != 0) { MessageBoxA(parent,"Erro ao ler histórico.","Erro",MB_OK|MB_ICONERROR); return; }
    if (n<=0) { MessageBoxA(parent,"Sem registos no histórico.","Histórico",MB_OK|MB_ICONINFORMATION); return; }

    /* MVP melhorado: menu de acção rápida por ID */
    {
        char head[1024];
        snprintf(head, sizeof(head), "Histórico com %d registos.\n\nPara já: edição/eliminação rápida por ID na próxima iteração.\nA mostrar primeiros 20 registos.", n);
        MessageBoxA(parent, head, "Histórico", MB_OK|MB_ICONINFORMATION);
    }

    {
        char buffer[8192]; int pos=0;
        for(i=0;i<n && i<20;i++) {
            pos += snprintf(buffer+pos, sizeof(buffer)-pos, "%d | %s | %s | %s\r\n", rows[i].id, rows[i].loan_date, rows[i].type, rows[i].title);
            if(pos > (int)sizeof(buffer)-128) break;
        }
        MessageBoxA(parent, buffer, "Histórico (Top 20)", MB_OK|MB_ICONINFORMATION);
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
        if(LOWORD(wParam)==ID_BTN_NEW){ new_record_data_t data; memset(&data,0,sizeof(data)); strcpy(data.type,"livro"); now_str(data.date); show_new_record_dialog(hwnd,&data); if(data.accepted){ if(loans_add(&ctx->cfg,data.title,data.date,data.type,NULL,0)!=0) MessageBoxA(hwnd,"Falha ao guardar registo.","Erro",MB_OK|MB_ICONERROR); refresh_dashboard(ctx);} return 0; }
        if(LOWORD(wParam)==ID_BTN_HISTORY){ show_history_window(hwnd,ctx); return 0; }
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
    hwnd=CreateWindowA("BiblioLEDWinXP","BiblioLED C99 (GUI Win32)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,900,560,NULL,NULL,hInst,NULL);
    if(!hwnd) return 2;
    ShowWindow(hwnd,nShow); UpdateWindow(hwnd);
    while(GetMessage(&msg,NULL,0,0)>0){ TranslateMessage(&msg); DispatchMessage(&msg);} return (int)msg.wParam;
}
