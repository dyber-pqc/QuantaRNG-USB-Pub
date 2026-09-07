/**
 * @file    platform_posix.c
 * @brief   POSIX (Linux/macOS/FreeBSD) backend for the QuantaRNG SDK.
 *
 * Treats the USB CDC device as a serial port via /dev/ttyACM* (Linux),
 * /dev/cu.usbmodem* (macOS), or /dev/cuaU* (FreeBSD).
 */

#if !defined(_WIN32)

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "quantarng.h"
#include "qrng_internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

#if defined(__APPLE__)
#  define DEV_DIR    "/dev"
#  define DEV_PREFIX "cu.usbmodem"
#elif defined(__FreeBSD__)
#  define DEV_DIR    "/dev"
#  define DEV_PREFIX "cuaU"
#else
#  define DEV_DIR    "/dev"
#  define DEV_PREFIX "ttyACM"
#endif

/* ----------------------------------------------------------------------- */

uint64_t qrng_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

/* ----------------------------------------------------------------------- */

#if defined(__linux__)

/* Read a single-line sysfs attribute, stripping the trailing newline. */
static int sysfs_attr(const char *dir, const char *attr,
                      char *out, size_t out_max)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, attr);

    FILE *f = fopen(path, "r");
    if (!f) { return 0; }

    int ok = (fgets(out, (int)out_max, f) != NULL);
    fclose(f);
    if (!ok) { return 0; }

    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

/*
 * Resolve the USB VID/PID (and serial, when exposed) backing a tty.
 *
 * /sys/class/tty/<name>/device points at the CDC *interface*; the
 * idVendor/idProduct attributes live on the parent USB *device*, so walk
 * up until they appear. Depth varies with hub topology, hence the loop.
 */
static int usb_ids_for_tty(const char *name,
                           uint16_t *vid, uint16_t *pid,
                           char *serial, size_t serial_max)
{
    char link[PATH_MAX];
    char dir[PATH_MAX];

    snprintf(link, sizeof(link), "/sys/class/tty/%s/device", name);
    if (!realpath(link, dir)) { return 0; }

    for (int depth = 0; depth < 8; depth++)
    {
        char v[32], p[32], s[128];

        if (sysfs_attr(dir, "idVendor", v, sizeof(v)) &&
            sysfs_attr(dir, "idProduct", p, sizeof(p)))
        {
            *vid = (uint16_t)strtoul(v, NULL, 16);
            *pid = (uint16_t)strtoul(p, NULL, 16);

            if (serial && serial_max > 0)
            {
                if (sysfs_attr(dir, "serial", s, sizeof(s)))
                {
                    /* Explicit precision: the USB serial may legitimately be
                     * longer than the field, and truncating is intended. */
                    snprintf(serial, serial_max, "%.*s",
                             (int)serial_max - 1, s);
                }
                else
                {
                    snprintf(serial, serial_max, "unknown");
                }
            }
            return 1;
        }

        char *slash = strrchr(dir, '/');
        if (!slash || slash == dir) { break; }
        *slash = '\0';
    }
    return 0;
}

#endif /* __linux__ */

/* ----------------------------------------------------------------------- */

static qrng_result_t configure_serial(int fd)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) { return QRNG_E_IO; }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;

    /* CDC ignores baud, but POSIX still wants it */
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);

    if (tcsetattr(fd, TCSANOW, &tio) != 0) { return QRNG_E_IO; }
    return QRNG_OK;
}

/* ----------------------------------------------------------------------- */

qrng_result_t qrng_enumerate(qrng_info_t *out_devices,
                             size_t       max,
                             size_t      *out_count)
{
    if (!out_devices || !out_count) { return QRNG_E_INVAL; }

    DIR *d = opendir(DEV_DIR);
    if (!d) { return QRNG_E_IO; }

    *out_count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && *out_count < max)
    {
        if (strncmp(e->d_name, DEV_PREFIX, strlen(DEV_PREFIX)) != 0)
        {
            continue;
        }
        qrng_info_t *info = &out_devices[*out_count];
        memset(info, 0, sizeof(*info));
        info->vid = QRNG_DEFAULT_VID;
        info->pid = QRNG_DEFAULT_PID;
        snprintf(info->serial, sizeof(info->serial), "unknown");

#if defined(__linux__)
        /* Only report genuine QuantaRNG devices: a Linux host commonly has
         * other CDC-ACM ports (GPS receivers, microcontroller boards, LTE
         * modems) that would otherwise be opened by mistake. */
        {
            uint16_t vid = 0, pid = 0;
            if (!usb_ids_for_tty(e->d_name, &vid, &pid,
                                 info->serial, sizeof(info->serial)))
            {
                continue;   /* not USB-backed, or sysfs unavailable */
            }
            if (vid != QRNG_DEFAULT_VID || pid != QRNG_DEFAULT_PID)
            {
                continue;   /* some other CDC device */
            }
            info->vid = vid;
            info->pid = pid;
        }
#endif

        /* Bounded precision keeps the result well inside info->path. */
        snprintf(info->path, sizeof(info->path), "%s/%.200s",
                 DEV_DIR, e->d_name);
        /* Requires opening the device; see qrng_get_firmware_version(). */
        snprintf(info->fw_version, sizeof(info->fw_version), "unknown");
        (*out_count)++;
    }
    closedir(d);
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

    int fd = open(chosen, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { return QRNG_E_OPEN; }

    qrng_result_t r = configure_serial(fd);
    if (r != QRNG_OK) { close(fd); return r; }

    qrng_device_t *dev = (qrng_device_t *)calloc(1, sizeof(*dev));
    if (!dev) { close(fd); return QRNG_E_NOMEM; }

    dev->fd = fd;
    dev->timeout_ms = 5000;
    snprintf(dev->path, sizeof(dev->path), "%s", chosen);

    *out = dev;
    return QRNG_OK;
}

void qrng_close(qrng_device_t *dev)
{
    if (!dev) { return; }
    if (dev->fd >= 0) { close(dev->fd); }
    free(dev);
}

void qrng_set_timeout(qrng_device_t *dev, uint32_t timeout_ms)
{
    if (dev) { dev->timeout_ms = timeout_ms; }
}

qrng_ssize_t qrng_read(qrng_device_t *dev, void *buf, size_t len)
{
    if (!dev || !buf) { return QRNG_E_INVAL; }

    struct pollfd pfd = { .fd = dev->fd, .events = POLLIN };
    int p = poll(&pfd, 1, (int)dev->timeout_ms);
    if (p < 0)  { return QRNG_E_IO; }
    if (p == 0) { return 0; }       /* timeout */

    ssize_t n = read(dev->fd, buf, len);
    if (n < 0)
    {
        return (errno == EAGAIN) ? 0 : QRNG_E_IO;
    }
    return (qrng_ssize_t)n;
}

qrng_result_t qrng_platform_write(qrng_device_t *dev,
                                  const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0)
    {
        ssize_t n = write(dev->fd, p, len);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EINTR) { continue; }
            return QRNG_E_IO;
        }
        p += n;
        len -= (size_t)n;
    }
    return QRNG_OK;
}

#endif /* !_WIN32 */
