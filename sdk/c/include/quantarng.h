/**
 * @file    quantarng.h
 * @brief   QuantaRNG USB C SDK — public API.
 *
 * Cross-platform C library for reading random data from a QuantaRNG USB
 * device. Supports Linux, macOS, Windows, and FreeBSD.
 *
 * The device appears to the OS as a CDC (virtual serial port) device.
 * This library hides platform-specific enumeration and I/O behind a
 * uniform handle-based API.
 *
 * @author  Dyber PQC
 * @copyright MIT
 */

#ifndef QUANTARNG_H
#define QUANTARNG_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  include <BaseTsd.h>
   typedef SSIZE_T qrng_ssize_t;
#else
#  include <sys/types.h>
   typedef ssize_t qrng_ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Library version (semver). Compare against @ref qrng_version() at runtime. */
#define QRNG_VERSION_MAJOR  1
#define QRNG_VERSION_MINOR  1
#define QRNG_VERSION_PATCH  0

/** Default USB Vendor ID. */
#define QRNG_DEFAULT_VID    0x0483
/** Default USB Product ID. */
#define QRNG_DEFAULT_PID    0x5740

/** Result codes returned by SDK functions. */
typedef enum
{
    QRNG_OK                  =  0,
    QRNG_E_NOT_FOUND         = -1,  /**< No matching device connected */
    QRNG_E_OPEN              = -2,  /**< Device-open failed (permission?) */
    QRNG_E_IO                = -3,  /**< Read/write transport error */
    QRNG_E_TIMEOUT           = -4,  /**< Read timed out */
    QRNG_E_HEALTH            = -5,  /**< Device reports health-test failure */
    QRNG_E_INVAL             = -6,  /**< Invalid argument */
    QRNG_E_NOMEM             = -7,
} qrng_result_t;

/** Opaque device handle. */
typedef struct qrng_device qrng_device_t;

/** Device descriptor returned by @ref qrng_enumerate(). */
typedef struct
{
    uint16_t vid;
    uint16_t pid;
    char     serial[64];
    char     path[256];          /**< OS-specific device path */
    char     fw_version[32];
} qrng_info_t;

/**
 * Health-test status returned by @ref qrng_get_health().
 *
 * @note The layout changed in SDK 1.1.0 (fields appended); the shared
 *       library SOVERSION was raised to 2 accordingly. Recompile callers
 *       against this header — do not mix a 1.1.0 library with a 1.0.0
 *       header, as the library writes the full struct.
 */
typedef struct
{
    uint64_t total_samples;      /**< ADC samples processed since power-on */
    uint32_t rct_failures;       /**< @deprecated Use @ref health_failures */
    uint32_t apt_failures;       /**< @deprecated Always 0 (see note below) */
    int      alarm;              /**< 1 while the pipeline is paused */

    /* --- added in 1.1.0 --- */
    uint64_t conditioned_bytes;  /**< Random bytes emitted since power-on */
    uint32_t health_failures;    /**< Combined RCT + APT trips since power-on */
    int      running;            /**< 1 if the entropy pipeline is active */
} qrng_health_status_t;

/* ============================== API =================================== */

/**
 * @brief  Get library version string ("1.0.0").
 */
const char *qrng_version(void);

/**
 * @brief  Enumerate all attached QuantaRNG devices.
 *
 * Devices are matched by USB VID/PID (Linux via sysfs, Windows via
 * SetupAPI), so unrelated CDC serial devices are not reported.
 *
 * @note On macOS and FreeBSD there is no VID/PID filtering: every
 *       CDC-ACM port is reported and @c vid / @c pid are reported as the
 *       defaults. Open an explicit path if other CDC devices are present.
 * @note @c fw_version is not populated here (it requires opening the
 *       device); use @ref qrng_get_firmware_version() after opening.
 *       @c serial is populated on Linux and Windows where available.
 *
 * @param  out_devices  Caller-allocated array.
 * @param  max          Capacity of @p out_devices.
 * @param  out_count    Receives the number of devices written.
 *
 * @return QRNG_OK on success, or an error code.
 */
qrng_result_t qrng_enumerate(qrng_info_t *out_devices,
                             size_t       max,
                             size_t      *out_count);

/**
 * @brief  Open a device by path (from @ref qrng_enumerate) or NULL for first.
 */
qrng_result_t qrng_open(const char *path, qrng_device_t **out);

/**
 * @brief  Close a device handle. Safe to call with NULL.
 */
void qrng_close(qrng_device_t *dev);

/**
 * @brief  Read up to @p len bytes of conditioned random data.
 *
 * Blocks until at least one byte is available or until the timeout
 * expires. Use @ref qrng_set_timeout to adjust.
 *
 * @return Number of bytes read, which may be fewer than @p len; @b 0 if
 *         the timeout expired with no data; or a negative
 *         @ref qrng_result_t on error. Always test @c < @c 0 for failure
 *         before treating the value as a length — a return of 0 is a
 *         timeout, not an error, and never indicates end-of-stream.
 *
 * @see    qrng_read_full() to read an exact count.
 */
qrng_ssize_t qrng_read(qrng_device_t *dev, void *buf, size_t len);

/**
 * @brief  Read exactly @p len bytes, blocking as long as needed (up to
 *         the configured timeout per call).
 *
 * @return QRNG_OK if @p len bytes were read; otherwise an error code.
 */
qrng_result_t qrng_read_full(qrng_device_t *dev, void *buf, size_t len);

/**
 * @brief  Set the per-read timeout in milliseconds. Default: 5000.
 */
void qrng_set_timeout(qrng_device_t *dev, uint32_t timeout_ms);

/**
 * @brief  Send a control command and read the reply.
 *
 * Commands: "STATUS", "VERSION", "REKEY", "DFU" (without a trailing
 * newline — one is appended).
 *
 * The device streams random data continuously and injects command replies
 * into that same stream, so this call scans for the command's reply
 * signature and discards the random bytes it passes over. Losing those
 * bytes is harmless: the stream is ergodic. Do not interleave
 * @ref qrng_read() from another thread on the same handle while a control
 * command is in flight, or the reply may be consumed by the reader.
 *
 * @param  reply      Receives the NUL-terminated reply; may be NULL to
 *                    send without waiting for one.
 * @param  reply_max  Capacity of @p reply.
 *
 * @return QRNG_OK, QRNG_E_TIMEOUT if no matching reply arrived within the
 *         configured timeout, or another error code.
 */
qrng_result_t qrng_control(qrng_device_t *dev,
                           const char    *cmd,
                           char          *reply,
                           size_t         reply_max);

/**
 * @brief  Convenience wrapper: query device firmware version.
 */
qrng_result_t qrng_get_firmware_version(qrng_device_t *dev,
                                        char *out, size_t out_max);

/**
 * @brief  Convenience wrapper: query device health-test status.
 */
qrng_result_t qrng_get_health(qrng_device_t *dev,
                              qrng_health_status_t *out);

/**
 * @brief  Trigger a key rotation in the conditioner.
 */
qrng_result_t qrng_rekey(qrng_device_t *dev);

/**
 * @brief  Reboot the device into USB DFU bootloader mode.
 *
 * After this call, the device disconnects and re-enumerates as a DFU
 * device. The handle is no longer valid.
 */
qrng_result_t qrng_enter_dfu(qrng_device_t *dev);

/* ============================== Errors ================================= */

/**
 * @brief  Get a human-readable description of a @ref qrng_result_t value.
 */
const char *qrng_strerror(qrng_result_t r);

#ifdef __cplusplus
}
#endif

#endif /* QUANTARNG_H */
