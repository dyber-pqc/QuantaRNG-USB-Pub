/*
 * Regression tests for command-reply extraction.
 *
 * The device interleaves ASCII replies with its continuous random stream,
 * so these tests feed the scanner synthetic streams — pseudo-random noise
 * with a reply embedded at an offset — and assert the reply is recovered
 * intact. Run without hardware.
 *
 * The scanner is exercised through its real entry point by stubbing the
 * two platform hooks it depends on (qrng_read, qrng_now_ms).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quantarng.h"
#include "qrng_internal.h"

/* ---- synthetic stream ------------------------------------------------- */

static unsigned char g_stream[64 * 1024];
static size_t        g_stream_len;
static size_t        g_stream_pos;
static uint64_t      g_now;

/* Deterministic PRNG so failures reproduce exactly. */
static uint32_t rng_state = 0x12345678u;
static unsigned char noise_byte(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (unsigned char)(rng_state & 0xff);
}

/* Build: <lead> noise bytes, then `reply`, then trailing noise. */
static void stream_build(const char *reply, size_t lead, size_t trail)
{
    size_t n = 0;
    for (size_t i = 0; i < lead; i++)      { g_stream[n++] = noise_byte(); }
    for (const char *p = reply; *p; p++)   { g_stream[n++] = (unsigned char)*p; }
    for (size_t i = 0; i < trail; i++)     { g_stream[n++] = noise_byte(); }
    g_stream_len = n;
    g_stream_pos = 0;
    g_now = 0;
}

/* ---- platform stubs --------------------------------------------------- */

uint64_t qrng_now_ms(void)
{
    return g_now;
}

qrng_ssize_t qrng_read(qrng_device_t *dev, void *buf, size_t len)
{
    (void)dev;
    g_now += 1;                       /* advance the clock on every read */

    if (g_stream_pos >= g_stream_len) { return 0; }   /* looks like a timeout */

    size_t avail = g_stream_len - g_stream_pos;
    size_t chunk = 64;                /* mimic 64-byte USB packets */
    if (chunk > avail) { chunk = avail; }
    if (chunk > len)   { chunk = len;   }

    memcpy(buf, g_stream + g_stream_pos, chunk);
    g_stream_pos += chunk;
    return (qrng_ssize_t)chunk;
}

/* Unused by these tests but required to link. */
qrng_result_t qrng_platform_write(qrng_device_t *dev, const void *buf, size_t len)
{
    (void)dev; (void)buf; (void)len;
    return QRNG_OK;
}

/* ---- harness ---------------------------------------------------------- */

static int g_failures;

static void check(const char *name, int ok, const char *detail)
{
    printf("%-46s %s%s%s\n", name, ok ? "PASS" : "FAIL",
           ok ? "" : "  <- ", ok ? "" : (detail ? detail : ""));
    if (!ok) { g_failures++; }
}

static qrng_result_t scan(qrng_match_kind_t kind, char *out, size_t out_max)
{
    qrng_device_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.timeout_ms = 5000;
    return qrng_scan_reply(&dev, kind, out, out_max);
}

int main(void)
{
    char out[256];

    /* VERSION buried in noise */
    stream_build("QuantaRNG 1.0.0\n", 900, 256);
    check("VERSION reply found in noisy stream",
          scan(QRNG_MATCH_VERSION, out, sizeof out) == QRNG_OK &&
          strcmp(out, "QuantaRNG 1.0.0") == 0, out);

    /* STATUS JSON buried in noise */
    stream_build("{\"running\":1,\"adc\":12345678,\"out\":1234567,\"hf\":0}\n",
                 1500, 512);
    check("STATUS reply found in noisy stream",
          scan(QRNG_MATCH_STATUS, out, sizeof out) == QRNG_OK &&
          strstr(out, "\"running\":1") != NULL &&
          strstr(out, "\"hf\":0") != NULL, out);

    /* REKEY acknowledgement */
    stream_build("OK\n", 700, 128);
    check("REKEY acknowledgement found in noisy stream",
          scan(QRNG_MATCH_OK, out, sizeof out) == QRNG_OK &&
          strcmp(out, "OK") == 0, out);

    /* Reply straddling the internal window slide */
    stream_build("QuantaRNG 9.9.9\n", 60000, 256);
    check("reply found beyond one window slide",
          scan(QRNG_MATCH_VERSION, out, sizeof out) == QRNG_OK &&
          strcmp(out, "QuantaRNG 9.9.9") == 0, out);

    /* Pure noise must not yield a false VERSION/STATUS match */
    stream_build("", 0, 60000);
    check("no false VERSION match in pure noise",
          scan(QRNG_MATCH_VERSION, out, sizeof out) == QRNG_E_TIMEOUT, out);

    stream_build("", 0, 60000);
    check("no false STATUS match in pure noise",
          scan(QRNG_MATCH_STATUS, out, sizeof out) == QRNG_E_TIMEOUT, out);

    /* A '{...}' span without "running" is not a STATUS reply */
    stream_build("{\"other\":1}\n", 500, 256);
    check("STATUS ignores JSON lacking \"running\"",
          scan(QRNG_MATCH_STATUS, out, sizeof out) == QRNG_E_TIMEOUT, out);

    /* DFU expects no reply and must return immediately */
    stream_build("", 0, 1024);
    check("DFU (no reply expected) returns OK",
          scan(QRNG_MATCH_NONE, out, sizeof out) == QRNG_OK, out);

    printf("\n%s (%d failure%s)\n",
           g_failures ? "FAILED" : "ALL TESTS PASSED",
           g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
