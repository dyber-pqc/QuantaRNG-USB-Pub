/**
 * @file    quantarng.c
 * @brief   Cross-platform shared logic for the QuantaRNG SDK.
 *
 * Platform-specific I/O is implemented in:
 *   - platform_posix.c   (Linux, macOS, FreeBSD)
 *   - platform_win32.c   (Windows)
 */

#include "quantarng.h"
#include "qrng_internal.h"

#include <stdio.h>
#include <string.h>

const char *qrng_version(void)
{
    return "1.1.0";
}

const char *qrng_strerror(qrng_result_t r)
{
    switch (r)
    {
    case QRNG_OK:           return "OK";
    case QRNG_E_NOT_FOUND:  return "device not found";
    case QRNG_E_OPEN:       return "failed to open device";
    case QRNG_E_IO:         return "I/O error";
    case QRNG_E_TIMEOUT:    return "operation timed out";
    case QRNG_E_HEALTH:     return "device reports health-test failure";
    case QRNG_E_INVAL:      return "invalid argument";
    case QRNG_E_NOMEM:      return "out of memory";
    default:                return "unknown error";
    }
}

qrng_result_t qrng_read_full(qrng_device_t *dev, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t   remaining = len;

    while (remaining > 0)
    {
        qrng_ssize_t r = qrng_read(dev, p, remaining);
        if (r < 0) { return (qrng_result_t)r; }
        if (r == 0) { return QRNG_E_TIMEOUT; }
        p += r;
        remaining -= (size_t)r;
    }
    return QRNG_OK;
}

qrng_result_t qrng_control(qrng_device_t *dev,
                           const char    *cmd,
                           char          *reply,
                           size_t         reply_max)
{
    if (!dev || !cmd) { return QRNG_E_INVAL; }

    char line[64];
    int  n = snprintf(line, sizeof(line), "%s\n", cmd);
    if (n <= 0 || (size_t)n >= sizeof(line)) { return QRNG_E_INVAL; }

    qrng_result_t r = qrng_platform_write(dev, line, (size_t)n);
    if (r != QRNG_OK) { return r; }

    if (!reply || reply_max == 0) { return QRNG_OK; }
    reply[0] = '\0';

    /* The reply arrives interleaved with the random stream, so scan for the
     * command's reply signature rather than reading the next bytes blindly. */
    return qrng_scan_reply(dev, qrng_match_kind_for(cmd), reply, reply_max);
}

qrng_result_t qrng_get_firmware_version(qrng_device_t *dev,
                                        char *out, size_t out_max)
{
    char buf[64];
    qrng_result_t r = qrng_control(dev, "VERSION", buf, sizeof(buf));
    if (r != QRNG_OK) { return r; }

    /* buf looks like "QuantaRNG 1.0.0\n" — strip prefix and newline */
    const char *space = strchr(buf, ' ');
    const char *src = space ? space + 1 : buf;
    size_t      len = strcspn(src, "\r\n");
    if (len >= out_max) { len = out_max - 1; }
    memcpy(out, src, len);
    out[len] = '\0';
    return QRNG_OK;
}

qrng_result_t qrng_get_health(qrng_device_t *dev,
                              qrng_health_status_t *out)
{
    if (!out) { return QRNG_E_INVAL; }

    char reply[256];
    qrng_result_t r = qrng_control(dev, "STATUS", reply, sizeof(reply));
    if (r != QRNG_OK) { return r; }

    /* Reply form: {"running":1,"adc":N,"out":N,"hf":N} */
    memset(out, 0, sizeof(*out));

    unsigned long long v = 0;
    const char *p;

    if ((p = strstr(reply, "\"running\":")) != NULL &&
        sscanf(p + 10, "%llu", &v) == 1)
    {
        out->running = (v != 0);
    }
    if ((p = strstr(reply, "\"adc\":")) != NULL &&
        sscanf(p + 6, "%llu", &v) == 1)
    {
        out->total_samples = (uint64_t)v;
    }
    if ((p = strstr(reply, "\"out\":")) != NULL &&
        sscanf(p + 6, "%llu", &v) == 1)
    {
        out->conditioned_bytes = (uint64_t)v;
    }
    if ((p = strstr(reply, "\"hf\":")) != NULL &&
        sscanf(p + 5, "%llu", &v) == 1)
    {
        out->health_failures = (uint32_t)v;
        /* Deprecated fields: the firmware reports a single combined counter,
         * so it is surfaced as rct_failures with apt_failures left at 0. */
        out->rct_failures = (uint32_t)v;
        out->apt_failures = 0;
    }

    /* The pipeline pauses itself while a health test is tripped. */
    out->alarm = out->running ? 0 : 1;
    return QRNG_OK;
}

qrng_result_t qrng_rekey(qrng_device_t *dev)
{
    char reply[16];
    return qrng_control(dev, "REKEY", reply, sizeof(reply));
}

qrng_result_t qrng_enter_dfu(qrng_device_t *dev)
{
    /* No reply expected; device disconnects */
    return qrng_control(dev, "DFU", NULL, 0);
}
