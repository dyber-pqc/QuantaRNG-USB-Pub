/*
 * Info / diagnostics example.
 *
 *   ./qrng-info
 *   QuantaRNG devices found: 1
 *   [0] /dev/ttyACM0  fw=1.0.0  serial=12345
 *       ADC samples: 1000000
 *       Health failures: 0
 */

#include <quantarng.h>

#include <stdio.h>

int main(void)
{
    qrng_info_t list[8];
    size_t n = 0;
    qrng_result_t r = qrng_enumerate(list, 8, &n);
    if (r != QRNG_OK)
    {
        fprintf(stderr, "enumerate: %s\n", qrng_strerror(r));
        return 1;
    }

    printf("QuantaRNG devices found: %zu\n", n);
    for (size_t i = 0; i < n; i++)
    {
        printf("[%zu] %s  serial=%s\n", i, list[i].path, list[i].serial);

        qrng_device_t *dev;
        qrng_result_t  r2 = qrng_open(list[i].path, &dev);
        if (r2 != QRNG_OK)
        {
            printf("    open: %s\n", qrng_strerror(r2));
            continue;
        }

        char fw[32];
        if (qrng_get_firmware_version(dev, fw, sizeof(fw)) == QRNG_OK)
        {
            printf("    Firmware: %s\n", fw);
        }

        qrng_health_status_t st;
        if (qrng_get_health(dev, &st) == QRNG_OK)
        {
            printf("    Pipeline: %s\n", st.running ? "running" : "PAUSED");
            printf("    ADC samples: %llu\n",
                   (unsigned long long)st.total_samples);
            printf("    Bytes emitted: %llu\n",
                   (unsigned long long)st.conditioned_bytes);
            printf("    Health failures: %u\n",
                   (unsigned int)st.health_failures);
        }
        qrng_close(dev);
    }
    return 0;
}
