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

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

HANDLE hid_open_by_usage(USHORT vid, USHORT pid, USHORT usage_page, wchar_t *mfr_out, wchar_t *prod_out, HIDP_CAPS *caps_out) {
    GUID guid;
    HidD_GetHidGuid(&guid);
    HDEVINFO di = SetupDiGetClassDevsA(&guid, NULL, NULL, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (di == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }
    SP_DEVICE_INTERFACE_DATA idata = {.cbSize = sizeof(idata)};
    HANDLE found = INVALID_HANDLE_VALUE;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(di, NULL, &guid, i, &idata); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailA(di, &idata, NULL, 0, &need, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *det = malloc(need);
        if (!det) {
            continue;
        }
        det->cbSize = sizeof(*det);
        if (!SetupDiGetDeviceInterfaceDetailA(di, &idata, det, need, NULL, NULL)) {
            free(det);
            continue;
        }
        HANDLE h = CreateFileA(det->DevicePath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        free(det);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        HIDD_ATTRIBUTES at = {.Size = sizeof(at)};
        if (!HidD_GetAttributes(h, &at) || at.VendorID != vid || at.ProductID != pid) {
            CloseHandle(h);
            continue;
        }
        PHIDP_PREPARSED_DATA ppd = NULL;
        if (HidD_GetPreparsedData(h, &ppd)) {
            HIDP_CAPS caps = {0};
            HidP_GetCaps(ppd, &caps);
            HidD_FreePreparsedData(ppd);
            if (caps.UsagePage == usage_page) {
                if (mfr_out) {
                    HidD_GetManufacturerString(h, mfr_out,  128*sizeof(wchar_t));
                }
                if (prod_out) {
                    HidD_GetProductString(h, prod_out, 128*sizeof(wchar_t));
                }
                if (caps_out) {
                    *caps_out = caps;
                }
                found = h;
                break;
            }
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(di);
    return found;
}

BOOL ov_write(HANDLE h, const void *buf, DWORD len, DWORD timeout_ms) {
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD written = 0;
    BOOL ok = WriteFile(h, buf, len, &written, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD w = WaitForSingleObject(ov.hEvent, timeout_ms);
        ok = (w == WAIT_OBJECT_0) && GetOverlappedResult(h, &ov, &written, FALSE);
    }
    CloseHandle(ov.hEvent);
    return ok && (written == len);
}

BOOL ov_read(HANDLE h, void *buf, DWORD len, DWORD timeout_ms) {
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, len, &got, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD w = WaitForSingleObject(ov.hEvent, timeout_ms);
        if (w == WAIT_OBJECT_0) {
            ok = GetOverlappedResult(h, &ov, &got, FALSE);
        } else {
            CancelIo(h);
        }
    }
    CloseHandle(ov.hEvent);
    return ok;
}
