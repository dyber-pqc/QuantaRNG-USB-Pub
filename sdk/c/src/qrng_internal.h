/**
 * @file    qrng_internal.h
 * @brief   Private SDK definitions shared between platform backends.
 */

#ifndef QRNG_INTERNAL_H
#define QRNG_INTERNAL_H

#include "quantarng.h"

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  include <windows.h>
   typedef HANDLE qrng_fd_t;
#else
   typedef int qrng_fd_t;
#endif

struct qrng_device
{
    qrng_fd_t  fd;
    uint32_t   timeout_ms;
    char       path[256];
    char       serial[64];
};

/** Reply signatures used to pick a command's reply out of the random stream. */
typedef enum
{
    QRNG_MATCH_NONE = 0,   /**< No reply expected (DFU) */
    QRNG_MATCH_VERSION,    /**< "QuantaRNG <semver>" */
    QRNG_MATCH_STATUS,     /**< JSON object containing "running" */
    QRNG_MATCH_OK,         /**< "OK" */
    QRNG_MATCH_LINE        /**< Generic printable line (best effort) */
} qrng_match_kind_t;

/** Map a command name to its reply signature. */
qrng_match_kind_t qrng_match_kind_for(const char *cmd);

/**
 * Consume the stream until @p kind's reply signature appears, or the
 * device timeout elapses. Random bytes read along the way are discarded.
 */
qrng_result_t qrng_scan_reply(qrng_device_t    *dev,
                              qrng_match_kind_t kind,
                              char             *out,
                              size_t            out_max);

/** Platform-specific write (blocking, returns QRNG_OK or error). */
qrng_result_t qrng_platform_write(qrng_device_t *dev,
                                  const void *buf, size_t len);

/** Monotonic milliseconds since an unspecified epoch (platform-specific). */
uint64_t qrng_now_ms(void);

#endif /* QRNG_INTERNAL_H */
