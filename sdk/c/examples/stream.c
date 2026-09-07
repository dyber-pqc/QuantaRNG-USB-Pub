/*
 * Continuous-stream example for QuantaRNG.
 *
 * Reads forever and writes to stdout, reporting throughput once per
 * second on stderr.
 *
 *   ./qrng-stream | dd of=/dev/null bs=1M count=100 status=progress
 */

#define _POSIX_C_SOURCE 200809L

#include <quantarng.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  include <windows.h>
static double now_sec(void)
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#else
static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}
#endif

int main(void)
{
    qrng_device_t *dev;
    qrng_result_t  r = qrng_open(NULL, &dev);
    if (r != QRNG_OK)
    {
        fprintf(stderr, "qrng_open: %s\n", qrng_strerror(r));
        return 1;
    }

    uint8_t buf[8192];
    uint64_t total = 0;
    double   t0 = now_sec();
    double   t_last = t0;
    uint64_t total_last = 0;

    for (;;)
    {
        qrng_ssize_t n = qrng_read(dev, buf, sizeof(buf));
        if (n < 0)
        {
            fprintf(stderr, "read: %s\n", qrng_strerror((qrng_result_t)n));
            break;
        }
        if (n == 0) { continue; }
        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) { break; }
        total += (uint64_t)n;

        double t = now_sec();
        if (t - t_last >= 1.0)
        {
            double inst = (total - total_last) / (t - t_last) / 1024.0;
            double avg  = total / (t - t0) / 1024.0;
            fprintf(stderr, "\r%.1f KiB/s (avg %.1f KiB/s, total %.1f MiB)",
                    inst, avg, total / (1024.0 * 1024.0));
            fflush(stderr);
            t_last = t;
            total_last = total;
        }
    }

    qrng_close(dev);
    return 0;
}
