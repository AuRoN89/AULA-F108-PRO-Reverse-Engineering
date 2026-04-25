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

static BOOL cmd_set(HANDLE hcmd, const uint8_t *pkt64) {
    uint8_t buf[65];
    buf[0] = 0x00;
    memcpy(buf+1, pkt64, 64);
    return HidD_SetFeature(hcmd, buf, 65);
}

static BOOL cmd_get(HANDLE hcmd, uint8_t *resp64) {
    uint8_t buf[65] = {0};
    buf[0] = 0x00;
    if (!HidD_GetFeature(hcmd, buf, 65)) return FALSE;
    if (resp64) memcpy(resp64, buf+1, 64);
    return TRUE;
}

static BOOL enter_flash_mode(HANDLE hcmd, HWND hwnd) {
    for (int attempt = 1; attempt <= REQUEST_FLASH_MODE; attempt++) {
        uint8_t p[CMD_PKT_SIZE] = {0};
        p[0]=CMD_BYTE0;
        p[1]=CMD_FLASH;
        if (!cmd_set(hcmd, p)) {
            Sleep(FLASH_RETRY_DELAY);
            continue;
        }
        print_log(hwnd, L"      waiting device confirmation... (attempt %d/%d)", attempt, REQUEST_FLASH_MODE);
        Sleep(FLASH_SETTLE_MS);
        uint8_t resp[CMD_PKT_SIZE] = {0};
        if (!cmd_get(hcmd, resp)) {
            Sleep(FLASH_RETRY_DELAY);
            continue;
        }
        if (resp[0] == CMD_BYTE0 && resp[1] == CMD_FLASH) {
            return TRUE;
        }
        Sleep(FLASH_RETRY_DELAY);
    }
    return FALSE;
}

static BOOL set_start_addr(HANDLE hcmd, uint32_t addr, uint32_t blocks) {
    uint8_t p[CMD_PKT_SIZE] = {0};
    p[0]=CMD_BYTE0;
    p[1]=CMD_ADDR;
    p[2]=(addr>>24)&0xFF;
    p[3]=(addr>>16)&0xFF;
    p[4]=(addr>> 8)&0xFF;
    p[5]= addr     &0xFF;
    p[6]=0x00;
    p[7]=0x00;
    p[8]= blocks       & 0xFF;
    p[9]=(blocks >> 8) & 0xFF;
    if (!cmd_set(hcmd, p)) {
        return FALSE;
    }
    uint8_t resp[CMD_PKT_SIZE] = {0};
    if (!cmd_get(hcmd, resp)) {
        return FALSE;
    }
    return (resp[0] == CMD_BYTE0 && resp[1] == CMD_ADDR);
}

static BOOL apply(HANDLE hcmd) {
    uint8_t p[CMD_PKT_SIZE] = {0};
    p[0]=CMD_BYTE0;
    p[1]=CMD_APPLY;
    return cmd_set(hcmd, p) && cmd_get(hcmd, NULL);
}

static BOOL finalize(HANDLE hcmd) {
    uint8_t p[CMD_PKT_SIZE] = {0};
    p[0]=CMD_BYTE0;
    p[1]=CMD_FINALIZE;
    return cmd_set(hcmd, p) && cmd_get(hcmd, NULL);
}

static BOOL write_blocks(HANDLE hdata, const uint8_t *write_block, USHORT in_report_size) {
    DWORD read_len = (in_report_size > 0) ? (DWORD)in_report_size : 65u;
    uint8_t *ack = malloc(read_len);
    if (!ack) {
        return FALSE;
    }

    OVERLAPPED ov_r = {0};
    ov_r.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD got = 0;
    ReadFile(hdata, ack, read_len, &got, &ov_r);

    uint8_t *buf = malloc(DATA_BLOCK_SIZE + 1);
    if (!buf) {
        CancelIo(hdata);
        CloseHandle(ov_r.hEvent);
        free(ack);
        return FALSE;
    }
    buf[0] = 0x00;
    memcpy(buf + 1, write_block, DATA_BLOCK_SIZE);
    BOOL wok = ov_write(hdata, buf, DATA_BLOCK_SIZE + 1, 2000);
    free(buf);

    if (!wok) {
        CancelIo(hdata);
        CloseHandle(ov_r.hEvent);
        free(ack);
        return FALSE;
    }

    BOOL ack_ok = (WaitForSingleObject(ov_r.hEvent, ACK_TIMEOUT_MS) == WAIT_OBJECT_0);
    if (ack_ok) {
        GetOverlappedResult(hdata, &ov_r, &got, FALSE);
    } else {
        CancelIo(hdata);
    }
    CloseHandle(ov_r.hEvent);
    free(ack);
    return ack_ok;
}

DWORD WINAPI flash_thread(LPVOID param) {
    FlashArgs *a = param;
    HWND hwnd = a->hwnd;
    HANDLE hcmd = a->hcmd;
    HANDLE hdata = a->hdata;
    uint8_t *fw = a->fw;
    size_t fw_size = a->fw_size;
    USHORT in_sz = a->in_report_size;
    free(a);

    print_log(hwnd, L"──────────────────────────────────────");
    print_log(hwnd, L"Firmware : %zu bytes (0x%zX)", fw_size, fw_size);

    size_t padded = fw_size;
    if (padded % DATA_BLOCK_SIZE) {
        padded += DATA_BLOCK_SIZE - (padded % DATA_BLOCK_SIZE);
    }
    uint8_t *buf = malloc(padded);
    if (!buf) {
        print_log(hwnd, L"[ERROR] Fatal Error");
        goto fail;
    }
    memcpy(buf, fw, fw_size);
    memset(buf + fw_size, 0xFF, padded - fw_size);
    free(fw);
    fw = NULL;

    uint32_t blocks = (uint32_t)(padded / DATA_BLOCK_SIZE);
    print_log(hwnd, L"Blocks   : %u x %u bytes", blocks, DATA_BLOCK_SIZE);

    print_log(hwnd, L"");
    print_log(hwnd, L"[1/4] Entering flash mode...");
    if (!enter_flash_mode(hcmd, hwnd)) {
        print_log(hwnd, L"[ERROR] Unable to enter flash mode after %d attempts", REQUEST_FLASH_MODE);
        goto fail;
    }
    print_log(hwnd, L"      OK");

    print_log(hwnd, L"");
    print_log(hwnd, L"[2/4] Start address 0x000000");
    if (!set_start_addr(hcmd, FLASH_START_ADDR, blocks)) {
        print_log(hwnd, L"[ERROR] CMD_ADDR failed");
        goto fail;
    }
    print_log(hwnd, L"      OK");
    Sleep(500);

    print_log(hwnd, L"");
    print_log(hwnd, L"[3/4] Writing %u blocks...", blocks);
    for (uint32_t pi = 0; pi < blocks; pi++) {
        if (!write_blocks(hdata, buf + pi * DATA_BLOCK_SIZE, in_sz)) {
            goto fail;
        }
        int pct = (int)((pi+1)*100/blocks);
        PostMessageW(hwnd, WM_FLASH_PCT, pct, 0);
        if (pi % 64 == 0 || pi == blocks-1) {
            print_log(hwnd, L"      Block %4u/%u  @ 0x%06X  [%3d%%]", pi+1, blocks, pi * DATA_BLOCK_SIZE, pct);
        }
    }
    print_log(hwnd, L"      OK");
    free(buf);
    buf = NULL;
    Sleep(500);

    print_log(hwnd, L"");
    print_log(hwnd, L"[4/4] Applying and finalizing...");
    if (!apply(hcmd)) {
        print_log(hwnd, L"      (applied but no ACK - may be OK)");
    }
    if (!finalize(hcmd)) {
        print_log(hwnd, L"      (finalized but no ACK - may be OK)");
    }
    print_log(hwnd, L"      OK");

    print_log(hwnd, L"");
    print_log(hwnd, L"Device rebooting...");
    Sleep(500);

    print_log(hwnd, L"");
    print_log(hwnd, L"Flash COMPLETE!");
    PostMessageW(hwnd, WM_FLASH_DONE, 1, 0);
    return 0;


    fail:
        free(buf);
        if (fw) {
            free(fw);
        }
        print_log(hwnd, L"");
        print_log(hwnd, L"Flash FAILED");
        PostMessageW(hwnd, WM_FLASH_DONE, 0, 0);
        return 1;
}
