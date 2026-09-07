/**
 * @file    qrng_reply.c
 * @brief   Command-reply extraction from the interleaved random stream.
 *
 * The device begins streaming conditioned random bytes as soon as the port
 * is opened and never stops. ASCII command replies are injected into that
 * same IN stream, so a naive "write command, read next line" will almost
 * always return random data instead of the reply (a random byte is '\n'
 * once every 256 bytes on average).
 *
 * Each command therefore has a distinctive reply signature, and this scanner
 * consumes the stream until that signature appears. Random bytes consumed
 * along the way are discarded — the protocol permits dropping stream bytes
 * (the stream is ergodic; dropping does not bias output).
 */

#include "quantarng.h"
#include "qrng_internal.h"

#include <stdlib.h>
#include <string.h>

#define QRNG_WINDOW_SIZE   8192u
/* Bytes of context preserved when the window slides. Must exceed the longest
 * possible reply so a reply is never split across a slide. */
#define QRNG_WINDOW_KEEP   1024u
#define QRNG_MAX_REPLY      512u

static int qrng_is_printable(unsigned char c)
{
    return c >= 0x20 && c <= 0x7e;
}

/* "QuantaRNG <semver>\n" — distinctive literal prefix, printable to newline. */
static int qrng_match_version(const char *w, size_t n,
                              size_t *off, size_t *len)
{
    static const char pfx[] = "QuantaRNG ";
    const size_t plen = sizeof(pfx) - 1;

    if (n < plen) { return 0; }

    for (size_t i = 0; i + plen <= n; i++)
    {
        if (memcmp(w + i, pfx, plen) != 0) { continue; }

        for (size_t j = i + plen; j < n && j - i < QRNG_MAX_REPLY; j++)
        {
            unsigned char c = (unsigned char)w[j];
            if (c == '\n')
            {
                size_t end = j;
                if (end > i && w[end - 1] == '\r') { end--; }
                *off = i;
                *len = end - i;
                return 1;
            }
            /* Non-printable inside the line: a chance hit in random data. */
            if (!qrng_is_printable(c)) { break; }
        }
    }
    return 0;
}

/* '{'...'}' span that is printable throughout and contains "running". */
static int qrng_match_status(const char *w, size_t n,
                             size_t *off, size_t *len)
{
    for (size_t i = 0; i < n; i++)
    {
        if (w[i] != '{') { continue; }

        for (size_t j = i + 1; j < n && j - i < QRNG_MAX_REPLY; j++)
        {
            unsigned char c = (unsigned char)w[j];
            if (c == '}')
            {
                size_t span = j - i + 1;
                char   tmp[QRNG_MAX_REPLY + 1];
                memcpy(tmp, w + i, span);
                tmp[span] = '\0';
                if (strstr(tmp, "\"running\"") != NULL)
                {
                    *off = i;
                    *len = span;
                    return 1;
                }
                break;
            }
            if (!qrng_is_printable(c)) { break; }
        }
    }
    return 0;
}

/* "OK\n" or "OK\r\n". Short signature — see note in qrng_scan_reply(). */
static int qrng_match_ok(const char *w, size_t n, size_t *off, size_t *len)
{
    for (size_t i = 0; i + 2 < n; i++)
    {
        if (w[i] != 'O' || w[i + 1] != 'K') { continue; }
        if (w[i + 2] == '\n' ||
            (w[i + 2] == '\r' && i + 3 < n && w[i + 3] == '\n'))
        {
            *off = i;
            *len = 2;
            return 1;
        }
    }
    return 0;
}

/* Fallback for commands this SDK does not know: any printable run of at
 * least 4 characters terminated by a newline. Best-effort only. */
static int qrng_match_line(const char *w, size_t n, size_t *off, size_t *len)
{
    size_t start = 0;
    size_t run   = 0;

    for (size_t i = 0; i < n; i++)
    {
        unsigned char c = (unsigned char)w[i];
        if (c == '\n')
        {
            if (run >= 4)
            {
                size_t end = i;
                if (end > start && w[end - 1] == '\r') { end--; }
                *off = start;
                *len = end - start;
                return 1;
            }
            run = 0;
        }
        else if (qrng_is_printable(c))
        {
            if (run == 0) { start = i; }
            run++;
            if (run > QRNG_MAX_REPLY) { run = 0; }
        }
        else
        {
            run = 0;
        }
    }
    return 0;
}

static int qrng_match(qrng_match_kind_t kind, const char *w, size_t n,
                      size_t *off, size_t *len)
{
    switch (kind)
    {
    case QRNG_MATCH_VERSION: return qrng_match_version(w, n, off, len);
    case QRNG_MATCH_STATUS:  return qrng_match_status(w, n, off, len);
    case QRNG_MATCH_OK:      return qrng_match_ok(w, n, off, len);
    case QRNG_MATCH_LINE:    return qrng_match_line(w, n, off, len);
    default:                 return 0;
    }
}

qrng_match_kind_t qrng_match_kind_for(const char *cmd)
{
    if (strcmp(cmd, "VERSION") == 0) { return QRNG_MATCH_VERSION; }
    if (strcmp(cmd, "STATUS")  == 0) { return QRNG_MATCH_STATUS;  }
    if (strcmp(cmd, "REKEY")   == 0) { return QRNG_MATCH_OK;      }
    if (strcmp(cmd, "DFU")     == 0) { return QRNG_MATCH_NONE;    }
    return QRNG_MATCH_LINE;
}

qrng_result_t qrng_scan_reply(qrng_device_t    *dev,
                              qrng_match_kind_t kind,
                              char             *out,
                              size_t            out_max)
{
    if (kind == QRNG_MATCH_NONE) { return QRNG_OK; }
    if (!dev) { return QRNG_E_INVAL; }

    char  *window = (char *)malloc(QRNG_WINDOW_SIZE);
    if (!window) { return QRNG_E_NOMEM; }

    size_t         wlen     = 0;
    const uint64_t deadline = qrng_now_ms() + dev->timeout_ms;
    qrng_result_t  result   = QRNG_E_TIMEOUT;

    for (;;)
    {
        size_t off = 0, len = 0;
        if (qrng_match(kind, window, wlen, &off, &len))
        {
            if (out && out_max > 0)
            {
                size_t cp = (len < out_max - 1) ? len : out_max - 1;
                memcpy(out, window + off, cp);
                out[cp] = '\0';
            }
            result = QRNG_OK;
            break;
        }

        if (qrng_now_ms() >= deadline) { result = QRNG_E_TIMEOUT; break; }

        /* Slide the window, preserving enough tail that a reply straddling
         * the boundary is still matchable on the next pass. */
        if (wlen > QRNG_WINDOW_SIZE - QRNG_WINDOW_KEEP)
        {
            memmove(window, window + wlen - QRNG_WINDOW_KEEP, QRNG_WINDOW_KEEP);
            wlen = QRNG_WINDOW_KEEP;
        }

        qrng_ssize_t rd = qrng_read(dev, window + wlen, QRNG_WINDOW_SIZE - wlen);
        if (rd < 0)  { result = (qrng_result_t)rd; break; }
        if (rd == 0) { continue; }   /* no data yet; deadline re-checked above */

        wlen += (size_t)rd;
    }

    free(window);
    return result;
}
