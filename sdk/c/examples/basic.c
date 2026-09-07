/*
 * Basic QuantaRNG SDK example.
 *
 * Opens the first attached device and reads 4 KiB of random data.
 *
 * Build:
 *   gcc basic.c -lquantarng -o qrng-basic
 *
 * Run:
 *   ./qrng-basic > random.bin
 */

#include <quantarng.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    qrng_device_t *dev = NULL;
    qrng_result_t  r   = qrng_open(NULL, &dev);
    if (r != QRNG_OK)
    {
        fprintf(stderr, "qrng_open: %s\n", qrng_strerror(r));
        return 1;
    }

    char fw[32];
    qrng_get_firmware_version(dev, fw, sizeof(fw));
    fprintf(stderr, "Connected to QuantaRNG (firmware %s)\n", fw);

    uint8_t buf[4096];
    r = qrng_read_full(dev, buf, sizeof(buf));
    if (r != QRNG_OK)
    {
        fprintf(stderr, "read: %s\n", qrng_strerror(r));
        qrng_close(dev);
        return 1;
    }

    fwrite(buf, 1, sizeof(buf), stdout);
    qrng_close(dev);
    return 0;
}
