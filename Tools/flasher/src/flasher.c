/******************************************************************************
 *  Aula F108 Pro Flasher
 *
 *  Copyright (c) 2025 Au{R}oN
 *
 *  Licensed under the Creative Commons Attribution 4.0 International License
 *  (CC BY 4.0).
 *
 *  You are free to:
 *  - Share: copy and redistribute the material in any medium or format
 *  - Adapt: remix, transform, and build upon the material for any purpose,
 *           even commercially
 *
 *  Under the following terms:
 *  - Attribution: You must give appropriate credit, provide a link to the
 *    license, and indicate if changes were made.
 *
 *  Full license text available at:
 *  https://creativecommons.org/licenses/by/4.0/
 *
 *  Description:
 *  SPI flashing tool for Aula F108 Pro keyboard.
 *
 *  Author: Au{R}oN
 *  Year:   2025
 *
 ******************************************************************************/

#include "../include/flasher.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

HWND   g_hwnd, g_edit_file;
HWND   g_btn_flash, g_btn_scan, g_progress, g_log;
HWND   g_lbl_status, g_lbl_dev;
HANDLE g_hcmd  = INVALID_HANDLE_VALUE;
HANDLE g_hdata = INVALID_HANDLE_VALUE;
BOOL   g_busy  = FALSE;

void print_line(const wchar_t *s) {
    int n = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, n, n);
    wchar_t buf[1100];
    swprintf_s(buf, 1100, L"%s\r\n", s);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
}

void print_log(HWND hwnd, const wchar_t *fmt, ...) {
    wchar_t *buf = HeapAlloc(GetProcessHeap(), 0, 2048*sizeof(wchar_t));
    if (!buf) return;
    va_list a;
    va_start(a, fmt);
    vswprintf_s(buf, 2048, fmt, a);
    va_end(a);
    PostMessageW(hwnd, WM_FLASH_LOG, 0, (LPARAM)buf);
}

static void scan_keyboard(void) {
    if (g_hcmd  != INVALID_HANDLE_VALUE) { 
        CloseHandle(g_hcmd);
        g_hcmd  = INVALID_HANDLE_VALUE;
    }
    if (g_hdata != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hdata);
        g_hdata = INVALID_HANDLE_VALUE;
    }

    wchar_t mfr[128]={0}, prod[128]={0};
    HIDP_CAPS cmd_caps={0}, data_caps={0};
    g_hcmd  = hid_open_by_usage(VID, PID, CMD,  mfr, prod,  &cmd_caps);
    g_hdata = hid_open_by_usage(VID, PID, DATA, NULL, NULL, &data_caps);

    if (g_hcmd == INVALID_HANDLE_VALUE || g_hdata == INVALID_HANDLE_VALUE) {
        wchar_t m[200];
        swprintf_s(m, 200, L"[ERROR] Device not found.");
        print_line(m);
        if (g_hcmd  != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hcmd);
            g_hcmd  = INVALID_HANDLE_VALUE;
        }
        if (g_hdata != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hdata);
            g_hdata = INVALID_HANDLE_VALUE;
        }
        SetWindowTextW(g_lbl_dev, L"No device");
        SetWindowTextW(g_lbl_status, L"Disconnected");
        EnableWindow(g_btn_flash, FALSE);
        return;
    }

    wchar_t info[256];
    swprintf_s(info, 256, L"%s  %s", mfr, prod);
    SetWindowTextW(g_lbl_dev, info);
    SetWindowTextW(g_lbl_status, L"Connected");
    wchar_t lm[300];
    swprintf_s(lm, 300, L"Device found: %s", info);
    print_line(lm);
    EnableWindow(g_btn_flash, TRUE);
}

static void load_file(void) {
    OPENFILENAMEW ofn={0};
    wchar_t path[MAX_PATH]={0};
    ofn.lStructSize=sizeof(ofn);
    ofn.hwndOwner=g_hwnd;
    ofn.lpstrFilter=L"*.bin\0*.bin\0All Files\0*.*\0\0";
    ofn.lpstrFile=path;
    ofn.nMaxFile=MAX_PATH;
    ofn.Flags=OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(g_edit_file, path);
    }
}

static void flash_keyboard(void) {
    if (g_busy) {
        return;
    }
    wchar_t path[MAX_PATH]={0};
    GetWindowTextW(g_edit_file, path, MAX_PATH);
    if (!path[0]) {
        MessageBoxW(g_hwnd, L"Select a .bin file.", L"", MB_ICONWARNING);
        return;
    }
    HANDLE fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fh == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwnd, L"Cannot open file.", L"Error", MB_ICONERROR);
        return;
    }
    DWORD fsz = GetFileSize(fh, NULL);
    uint8_t *fw = malloc(fsz);
    DWORD rd = 0;
    ReadFile(fh, fw, fsz, &rd, NULL);
    CloseHandle(fh);
    if (rd != fsz) {
        free(fw);
        MessageBoxW(g_hwnd, L"Read error.", L"Error", MB_ICONERROR);
        return;
    }
    wchar_t msg[512];
    swprintf_s(msg, 512, L"Flash %lu bytes?\n\nDo NOT disconnect during flashing.", fsz);
    if (MessageBoxW(g_hwnd, msg, L"Confirm", MB_YESNO|MB_ICONQUESTION) != IDYES) {
        free(fw);
        return;
    }

    g_busy = TRUE;
    EnableWindow(g_btn_flash, FALSE);
    EnableWindow(g_btn_scan, FALSE);
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    SetWindowTextW(g_lbl_status, L"Flashing...");

    HIDP_CAPS data_caps={0};
    PHIDP_PREPARSED_DATA ppd=NULL;
    if (HidD_GetPreparsedData(g_hdata, &ppd)) {
        HidP_GetCaps(ppd, &data_caps);
        HidD_FreePreparsedData(ppd);
    }

    FlashArgs *a = malloc(sizeof(FlashArgs));
    a->hwnd = g_hwnd;
    a->hcmd = g_hcmd;
    a->hdata = g_hdata;
    a->fw = fw;
    a->fw_size = fsz;
    a->in_report_size = data_caps.InputReportByteLength;
    CloseHandle(CreateThread(NULL, 0, flash_thread, a, 0, NULL));
}

#define MK(cls,txt,st,x,y,w,h,id) CreateWindowW(cls,txt,WS_CHILD|WS_VISIBLE|(st),x,y,w,h,hwnd,(HMENU)(id),NULL,NULL)
#define SF(hw) SendMessageW(hw,WM_SETFONT,(WPARAM)hf,TRUE)

/* ── WndProc ─────────────────────────────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hm = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,FIXED_PITCH,L"Courier New");
        SF(MK(L"STATIC",  L"File:", 0,                                       10,  13,  30,  18, 0));
        g_edit_file  = MK(L"EDIT",  L"", WS_BORDER|ES_AUTOHSCROLL,           42,  10, 268,  22,    IDC_EDIT_FILE);
        SF(g_edit_file);

        SF(MK(L"BUTTON",  L"Load Bin",   BS_PUSHBUTTON,                     318,  10,  80,  22,    IDC_BTN_BROWSE));
        g_btn_scan   = MK(L"BUTTON",L"Scan",  BS_PUSHBUTTON,                 10,  42,  60,  22,    IDC_BTN_SCAN);
        SF(g_btn_scan);

        g_lbl_dev    = MK(L"STATIC",L"No device", SS_SUNKEN,                 80,  44, 228,  18,    IDC_LBL_DEV);
        SF(g_lbl_dev);

        g_btn_flash  = MK(L"BUTTON",L"Flash Bin", BS_PUSHBUTTON,            318,  42,  80,  22,    IDC_BTN_FLASH);
        SF(g_btn_flash);

        EnableWindow(g_btn_flash, FALSE);
        g_lbl_status = MK(L"STATIC",L"Disconnected", 0,                      10,  76, 160,  20,    IDC_LBL_STATUS);
        SF(g_lbl_status);
        
        g_progress   = MK(PROGRESS_CLASSW, NULL, PBS_SMOOTH,                178,  78, 220,  16,    IDC_PROGRESS);
        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        g_log = MK(L"EDIT", L"", WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL, 10, 106, 398, 244, IDC_LOG);
        SendMessageW(g_log, WM_SETFONT, (WPARAM)hm, TRUE);
        print_line(L"Load .bin file, scan device and then flash!");
        print_line(L"──────────────────────────────────────────");
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
            case IDC_BTN_SCAN:
                scan_keyboard();
                break;
            case IDC_BTN_BROWSE:
                load_file();
                break;
            case IDC_BTN_FLASH:
                flash_keyboard();
                break;
        }
        break;
    case WM_FLASH_LOG:
        wchar_t *s=(wchar_t*)lp;
        if (s) {
            print_line(s);
            HeapFree(GetProcessHeap(),0,s);
        }
        break;
    case WM_FLASH_PCT:
        SendMessageW(g_progress, PBM_SETPOS, wp, 0);
        break;
    case WM_FLASH_DONE:
        g_busy = FALSE;
        EnableWindow(g_btn_scan, TRUE);
        if (g_hcmd != INVALID_HANDLE_VALUE && g_hdata != INVALID_HANDLE_VALUE) {
            EnableWindow(g_btn_flash, TRUE);
        }
        SetWindowTextW(g_lbl_status, wp ? L"Done" : L"Error");
        break;
    case WM_DESTROY:
        if (g_hcmd  != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hcmd);
        }
        if (g_hdata != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hdata);
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cl, int ns) {
    (void)hp;
    (void)cl;
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&ic);
    WNDCLASSW wc = {
        .style         = CS_HREDRAW|CS_VREDRAW,
        .lpfnWndProc   = WndProc,
        .hInstance     = hi,
        .hCursor       = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
        .lpszClassName = L"F108PRO_FLASHER",
    };
    RegisterClassW(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 420, wh = 400;
    g_hwnd = CreateWindowW(L"F108PRO_FLASHER", L"AULA F108 Pro SPI Flasher by Au{R}oN", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, (sw-ww)/2, (sh-wh)/2, ww, wh, NULL, NULL, hi, NULL);
    ShowWindow(g_hwnd, ns);
    UpdateWindow(g_hwnd);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
