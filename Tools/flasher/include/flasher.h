#pragma once

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define VID                 0x0C45
#define PID                 0x800A
#define CMD                 0xFF13
#define DATA                0xFF68

#define CMD_PKT_SIZE        64
#define CMD_BYTE0           0x04
#define CMD_FLASH           0x18
#define CMD_ADDR            0x38
#define CMD_APPLY           0x02
#define CMD_FINALIZE        0xF0
#define DATA_BLOCK_SIZE     4096
#define ACK_TIMEOUT_MS      5000
#define FLASH_START_ADDR    0x00000000u
#define FLASH_SETTLE_MS     1500
#define REQUEST_FLASH_MODE  5
#define FLASH_RETRY_DELAY   500

#define WM_FLASH_LOG        (WM_USER+1)
#define WM_FLASH_PCT        (WM_USER+2)
#define WM_FLASH_DONE       (WM_USER+3)

#define IDC_EDIT_FILE       103
#define IDC_BTN_BROWSE      104
#define IDC_BTN_SCAN        105
#define IDC_BTN_FLASH       106
#define IDC_PROGRESS        107
#define IDC_LOG             108
#define IDC_LBL_STATUS      109
#define IDC_LBL_DEV         110

typedef struct {
    HWND   hwnd;
    HANDLE hcmd;
    HANDLE hdata;
    uint8_t *fw;
    size_t   fw_size;
    USHORT   in_report_size;
} FlashArgs;

HANDLE hid_open_by_usage(USHORT vid, USHORT pid, USHORT usage_page, wchar_t *mfr_out, wchar_t *prod_out, HIDP_CAPS *caps_out);
BOOL ov_write(HANDLE h, const void *buf, DWORD len, DWORD timeout_ms);
BOOL ov_read (HANDLE h, void *buf, DWORD len, DWORD timeout_ms);

DWORD WINAPI flash_thread(LPVOID param);

void print_line(const wchar_t *s);
void print_log(HWND hwnd, const wchar_t *fmt, ...);
