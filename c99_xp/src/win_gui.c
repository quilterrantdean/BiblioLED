#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "config_store.h"
#include "loans_repo.h"
#include "metrics.h"

#define ID_BTN_REFRESH 1001
#define ID_BTN_NEW     1002
#define ID_LBL_STATUS  2001
#define ID_LIST_EXP    3001

typedef struct {
    app_config_t cfg;
    HWND h_status;
    HWND h_list;
} app_ctx_t;

static void set_status(HWND h, const char *msg) {
    SetWindowTextA(h, msg ? msg : "");
}

static void refresh_dashboard(app_ctx_t *ctx) {
    dashboard_metrics_t m;
    char line[512];
    int i;

    if (metrics_get_dashboard(&ctx->cfg, &m, NULL, 0) != 0) {
        set_status(ctx->h_status, "Erro ao calcular métricas.");
        return;
    }

    ListView_DeleteAllItems(ctx->h_list);

    for (i = 0; i < m.expiries_count; ++i) {
        LVITEMA it;
        char days[32];
        ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = m.expiries[i].title;
        ListView_InsertItemA(ctx->h_list, &it);
        ListView_SetItemText(ctx->h_list, i, 1, m.expiries[i].expiry_date);
        sprintf(days, "%d", m.expiries[i].days_left);
        ListView_SetItemText(ctx->h_list, i, 2, days);
        ListView_SetItemText(ctx->h_list, i, 3, m.expiries[i].type);
    }

    sprintf(line, "Disponíveis hoje: %d | Disponíveis mês: %d | Expirações: %d",
            m.daily_available, m.monthly_available, m.expiries_count);
    set_status(ctx->h_status, line);
}

static int prompt_new_record(HWND hwnd, app_ctx_t *ctx) {
    /* MVP: para dev/teste local, adiciona um registo de exemplo. */
    if (loans_add(&ctx->cfg, "Novo título (MVP)", "2026-05-19 12:00", "livro", NULL, 0) != 0) {
        MessageBoxA(hwnd, "Falha ao criar registo.", "Erro", MB_ICONERROR | MB_OK);
        return 1;
    }
    MessageBoxA(hwnd, "Registo de teste criado (MVP).", "Informação", MB_ICONINFORMATION | MB_OK);
    refresh_dashboard(ctx);
    return 0;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    app_ctx_t *ctx = (app_ctx_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        LVCOLUMNA col;
        RECT rc;
        HWND btn_refresh, btn_new;
        app_ctx_t *new_ctx = (app_ctx_t*)calloc(1, sizeof(app_ctx_t));
        if (!new_ctx) return -1;

        config_load_user_settings("user_settings.json", &new_ctx->cfg, NULL, 0);
        loans_init_db(&new_ctx->cfg, NULL, 0);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new_ctx);
        ctx = new_ctx;

        btn_refresh = CreateWindowA("BUTTON", "Actualizar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 100, 26, hwnd, (HMENU)ID_BTN_REFRESH, GetModuleHandle(NULL), NULL);
        btn_new = CreateWindowA("BUTTON", "Novo Registo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 10, 120, 26, hwnd, (HMENU)ID_BTN_NEW, GetModuleHandle(NULL), NULL);
        (void)btn_refresh; (void)btn_new;

        GetClientRect(hwnd, &rc);
        ctx->h_list = CreateWindowA(WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 45, rc.right - 20, rc.bottom - 90, hwnd, (HMENU)ID_LIST_EXP, GetModuleHandle(NULL), NULL);

        ListView_SetExtendedListViewStyle(ctx->h_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = "Título"; col.cx = 300; ListView_InsertColumn(ctx->h_list, 0, &col);
        col.pszText = "Expira em"; col.cx = 140; ListView_InsertColumn(ctx->h_list, 1, &col);
        col.pszText = "Dias"; col.cx = 70; ListView_InsertColumn(ctx->h_list, 2, &col);
        col.pszText = "Tipo"; col.cx = 110; ListView_InsertColumn(ctx->h_list, 3, &col);

        ctx->h_status = CreateWindowA("STATIC", "A carregar...", WS_CHILD | WS_VISIBLE,
            10, rc.bottom - 35, rc.right - 20, 20, hwnd, (HMENU)ID_LBL_STATUS, GetModuleHandle(NULL), NULL);

        refresh_dashboard(ctx);
        return 0;
    }
    case WM_SIZE:
        if (ctx && ctx->h_list && ctx->h_status) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(ctx->h_list, 10, 45, rc.right - 20, rc.bottom - 90, TRUE);
            MoveWindow(ctx->h_status, 10, rc.bottom - 35, rc.right - 20, 20, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (!ctx) return 0;
        switch (LOWORD(wParam)) {
        case ID_BTN_REFRESH:
            refresh_dashboard(ctx);
            break;
        case ID_BTN_NEW:
            prompt_new_record(hwnd, ctx);
            break;
        }
        return 0;
    case WM_DESTROY:
        if (ctx) free(ctx);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    (void)hPrev; (void)lpCmd;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = "BiblioLEDWinXP";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) return 1;

    hwnd = CreateWindowA("BiblioLEDWinXP", "BiblioLED C99 (MVP GUI)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 850, 520,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 2;

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
