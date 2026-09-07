/**
 * @file    platform_win32.c
 * @brief   Windows backend for the QuantaRNG SDK.
 *
 * Treats the USB CDC device as a virtual COM port. Enumeration walks
 * the SetupAPI device class for COM ports filtered by VID/PID.
 */

#if defined(_WIN32)

#include "quantarng.h"
#include "qrng_internal.h"

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "setupapi.lib")

/* ----------------------------------------------------------------------- */

uint64_t qrng_now_ms(void)
{
    return (uint64_t)GetTickCount64();
}

/* ----------------------------------------------------------------------- */

static qrng_result_t configure_com(HANDLE h, uint32_t timeout_ms)
{
    DCB dcb = { .DCBlength = sizeof(dcb) };
    if (!GetCommState(h, &dcb)) { return QRNG_E_IO; }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;

    if (!SetCommState(h, &dcb)) { return QRNG_E_IO; }

    COMMTIMEOUTS to = {
        .ReadIntervalTimeout         = MAXDWORD,
        .ReadTotalTimeoutMultiplier  = 0,
        .ReadTotalTimeoutConstant    = timeout_ms,
        .WriteTotalTimeoutMultiplier = 0,
        .WriteTotalTimeoutConstant   = timeout_ms,
    };
    if (!SetCommTimeouts(h, &to)) { return QRNG_E_IO; }

    return QRNG_OK;
}

/* ----------------------------------------------------------------------- */

qrng_result_t qrng_enumerate(qrng_info_t *out_devices,
                             size_t       max,
                             size_t      *out_count)
{
    if (!out_devices || !out_count) { return QRNG_E_INVAL; }

    *out_count = 0;
    HDEVINFO h = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) { return QRNG_E_IO; }

    SP_DEVINFO_DATA info = { .cbSize = sizeof(info) };
    char hwid[256];
    char friendly[256];
    char vid_pid[32];
    snprintf(vid_pid, sizeof(vid_pid),
             "VID_%04X&PID_%04X", QRNG_DEFAULT_VID, QRNG_DEFAULT_PID);

    for (DWORD i = 0;
         SetupDiEnumDeviceInfo(h, i, &info) && *out_count < max;
         i++)
    {
        if (!SetupDiGetDeviceRegistryPropertyA(
                h, &info, SPDRP_HARDWAREID, NULL,
                (BYTE *)hwid, sizeof(hwid), NULL))
        {
            continue;
        }
        if (strstr(hwid, vid_pid) == NULL) { continue; }

        if (!SetupDiGetDeviceRegistryPropertyA(
                h, &info, SPDRP_FRIENDLYNAME, NULL,
                (BYTE *)friendly, sizeof(friendly), NULL))
        {
            continue;
        }

        /* Friendly name contains "(COMx)" — extract */
        const char *p = strstr(friendly, "(COM");
        if (!p) { continue; }
        char com[16];
        sscanf(p, "(%15[^)])", com);

        qrng_info_t *out = &out_devices[*out_count];
        memset(out, 0, sizeof(*out));
        out->vid = QRNG_DEFAULT_VID;
        out->pid = QRNG_DEFAULT_PID;
        snprintf(out->path, sizeof(out->path), "\\\\.\\%s", com);
        strncpy(out->serial, "unknown", sizeof(out->serial) - 1);
        strncpy(out->fw_version, "unknown", sizeof(out->fw_version) - 1);
        (*out_count)++;
    }

    SetupDiDestroyDeviceInfoList(h);
    return QRNG_OK;
}

qrng_result_t qrng_open(const char *path, qrng_device_t **out)
{
    if (!out) { return QRNG_E_INVAL; }

    char chosen[256];
    if (path)
    {
        strncpy(chosen, path, sizeof(chosen) - 1);
        chosen[sizeof(chosen) - 1] = '\0';
    }
    else
    {
        qrng_info_t list[8];
        size_t n = 0;
        qrng_result_t r = qrng_enumerate(list, 8, &n);
        if (r != QRNG_OK) { return r; }
        if (n == 0) { return QRNG_E_NOT_FOUND; }
        snprintf(chosen, sizeof(chosen), "%s", list[0].path);
    }

    HANDLE h = CreateFileA(chosen, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { return QRNG_E_OPEN; }

    qrng_device_t *dev = (qrng_device_t *)calloc(1, sizeof(*dev));
    if (!dev) { CloseHandle(h); return QRNG_E_NOMEM; }
    dev->fd = h;
    dev->timeout_ms = 5000;
    snprintf(dev->path, sizeof(dev->path), "%s", chosen);

    qrng_result_t r = configure_com(h, dev->timeout_ms);
    if (r != QRNG_OK) { qrng_close(dev); return r; }

    *out = dev;
    return QRNG_OK;
}

void qrng_close(qrng_device_t *dev)
{
    if (!dev) { return; }
    if (dev->fd != INVALID_HANDLE_VALUE) { CloseHandle(dev->fd); }
    free(dev);
}

void qrng_set_timeout(qrng_device_t *dev, uint32_t timeout_ms)
{
    if (!dev) { return; }
    dev->timeout_ms = timeout_ms;
    configure_com(dev->fd, timeout_ms);
}

qrng_ssize_t qrng_read(qrng_device_t *dev, void *buf, size_t len)
{
    if (!dev || !buf) { return QRNG_E_INVAL; }

    DWORD got = 0;
    if (!ReadFile(dev->fd, buf, (DWORD)len, &got, NULL))
    {
        return QRNG_E_IO;
    }
    return (qrng_ssize_t)got;
}

qrng_result_t qrng_platform_write(qrng_device_t *dev,
                                  const void *buf, size_t len)
{
    DWORD wrote = 0;
    if (!WriteFile(dev->fd, buf, (DWORD)len, &wrote, NULL))
    {
        return QRNG_E_IO;
    }
    return (wrote == len) ? QRNG_OK : QRNG_E_IO;
}

#endif /* _WIN32 */
