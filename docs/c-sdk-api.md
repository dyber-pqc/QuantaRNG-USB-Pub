# QuantaRNG USB — C SDK API Reference

**`libquantarng` — integrating QuantaRNG into native C applications**

*Dyber, Inc. — SDK 1.1.0 — applies to firmware 1.0.x*

---

## 1. Overview

`libquantarng` is a small C11 library that hides the platform differences behind reading random data from a QuantaRNG USB device. The device is a USB CDC (virtual serial port) peripheral, so the library works on any OS with a CDC class driver — no kernel module, no libusb, no vendor driver.

| Platform | Device path | Enumeration |
|---|---|---|
| Linux (x86-64, aarch64, armhf) | `/dev/ttyACM*` | USB VID/PID via sysfs |
| macOS (Apple Silicon, Intel) | `/dev/cu.usbmodem*` | Path glob (see §7) |
| Windows 10/11 | `\\.\COM*` | USB VID/PID via SetupAPI |
| FreeBSD 13+ | `/dev/cuaU*` | Path glob (see §7) |

The library has **no external dependencies** — only libc and, on Windows, `setupapi`. It builds with GCC, Clang, and MSVC.

**Source:** `sdk/c/` in this repository. **License:** MIT.

---

## 2. Quick Start

```c
#include <quantarng.h>
#include <stdio.h>

int main(void)
{
    qrng_device_t *dev = NULL;

    qrng_result_t r = qrng_open(NULL, &dev);     /* NULL = first device */
    if (r != QRNG_OK) {
        fprintf(stderr, "open: %s\n", qrng_strerror(r));
        return 1;
    }

    unsigned char key[32];
    r = qrng_read_full(dev, key, sizeof key);    /* exactly 32 bytes */
    if (r != QRNG_OK) {
        fprintf(stderr, "read: %s\n", qrng_strerror(r));
        qrng_close(dev);
        return 1;
    }

    for (size_t i = 0; i < sizeof key; i++) printf("%02x", key[i]);
    printf("\n");

    qrng_close(dev);
    return 0;
}
```

Build:

```bash
gcc app.c $(pkg-config --cflags --libs quantarng) -o app
```

or without pkg-config:

```bash
gcc app.c -lquantarng -o app
```

---

## 3. Building the Library

### 3.1 Native build

```bash
cd sdk/c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

Installs `libquantarng.so.1.1.0` (with `.so.2` / `.so` symlinks), `quantarng.h`, and `quantarng.pc`.

| CMake option | Default | Effect |
|---|---|---|
| `BUILD_SHARED` | `ON` | `OFF` produces a static `libquantarng.a` |
| `BUILD_EXAMPLES` | `ON` | Builds `qrng-basic`, `qrng-stream`, `qrng-info` |
| `BUILD_TESTS` | `OFF` | Builds unit tests |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Install root |

### 3.2 Cross-compiling for ARM (Raspberry Pi and similar)

Raspberry Pi OS / Debian **Bookworm** ships in two flavours; pick the one matching `uname -m` on the target:

| Target `uname -m` | Architecture | Toolchain package |
|---|---|---|
| `aarch64` | 64-bit ARM (arm64) | `gcc-aarch64-linux-gnu` |
| `armv7l` / `armv6l` | 32-bit ARM (armhf) | `gcc-arm-linux-gnueabihf` |

> **Note:** "ARM64" and "32-bit" are mutually exclusive — ARM64 (`aarch64`) is a 64-bit architecture. If `uname -m` reports `armv7l`, you have a 32-bit userland and need the **armhf** build even if the CPU is 64-bit capable.

On a Debian/Ubuntu build host:

```bash
sudo apt-get install gcc-aarch64-linux-gnu      # for arm64
sudo apt-get install gcc-arm-linux-gnueabihf    # for armhf
```

**arm64 (aarch64):**

```bash
cd sdk/c
cmake -B build-arm64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
cmake --build build-arm64 -j
```

**armhf (32-bit):**

```bash
cd sdk/c
cmake -B build-armhf \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=arm \
      -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc
cmake --build build-armhf -j
```

Verify what you produced before shipping it to the target:

```bash
file build-arm64/libquantarng.so.1.1.0
# ELF 64-bit LSB shared object, ARM aarch64, version 1 (SYSV), dynamically linked
```

**Building natively on the Pi** works too and needs no toolchain setup:

```bash
sudo apt-get install cmake build-essential
cd sdk/c && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

Prebuilt `aarch64` and `armhf` binaries are attached to every [release](https://github.com/dyber-pqc/QuantaRNG-USB-Pub/releases/latest).

---

## 4. API Reference

All functions are declared in `quantarng.h` and are `extern "C"`-safe for C++ callers.

### 4.1 Result codes

Every fallible call returns `qrng_result_t`. Values are **negative on failure**, `QRNG_OK` (0) on success.

| Code | Value | Meaning |
|---|---|---|
| `QRNG_OK` | 0 | Success |
| `QRNG_E_NOT_FOUND` | −1 | No matching device connected |
| `QRNG_E_OPEN` | −2 | Open failed — usually permissions (§6) or the port is already open |
| `QRNG_E_IO` | −3 | Transport error; the device may have been unplugged |
| `QRNG_E_TIMEOUT` | −4 | No data (or no reply) within the configured timeout |
| `QRNG_E_HEALTH` | −5 | Reserved for health-test failures |
| `QRNG_E_INVAL` | −6 | Invalid argument (NULL pointer, oversized command) |
| `QRNG_E_NOMEM` | −7 | Allocation failed |

```c
const char *qrng_strerror(qrng_result_t r);
```

Returns a static, human-readable string. Never NULL; never needs freeing.

---

### 4.2 Version

```c
const char *qrng_version(void);
```

Returns the **library** version as a static string, e.g. `"1.1.0"` — not the device firmware version (see `qrng_get_firmware_version`). Compile-time constants `QRNG_VERSION_MAJOR` / `_MINOR` / `_PATCH` are also available; compare them against `qrng_version()` to catch a header/library mismatch at startup.

---

### 4.3 Enumeration

```c
typedef struct {
    uint16_t vid;
    uint16_t pid;
    char     serial[64];
    char     path[256];        /* OS-specific device path */
    char     fw_version[32];
} qrng_info_t;

qrng_result_t qrng_enumerate(qrng_info_t *out_devices,
                             size_t       max,
                             size_t      *out_count);
```

Fills a **caller-allocated** array with descriptors for attached devices. Writes at most `max` entries and stores the count in `*out_count`. Returns `QRNG_OK` even when zero devices are found — check `*out_count`.

- On **Linux** and **Windows**, results are filtered by USB VID/PID (`0483:5740`), so unrelated CDC devices (GPS receivers, microcontroller boards, LTE modems) are **not** returned.
- On **macOS/FreeBSD**, no VID/PID filtering is performed — every CDC port is listed. Open an explicit path if other CDC devices may be present.
- `serial` is populated on Linux and Windows where the OS exposes it, otherwise `"unknown"`.
- `fw_version` is **not** populated here — obtaining it requires opening the device. Use `qrng_get_firmware_version()` after `qrng_open()`.

```c
qrng_info_t list[8];
size_t n = 0;

if (qrng_enumerate(list, 8, &n) == QRNG_OK) {
    for (size_t i = 0; i < n; i++)
        printf("[%zu] %s  serial=%s\n", i, list[i].path, list[i].serial);
}
```

---

### 4.4 Open and close

```c
qrng_result_t qrng_open(const char *path, qrng_device_t **out);
void          qrng_close(qrng_device_t *dev);
```

`qrng_open()` opens a device by `path` (as returned by `qrng_enumerate()`), or the first device found when `path` is `NULL`. On success `*out` receives an opaque handle.

Passing an explicit path always works regardless of enumeration — this is the escape hatch on platforms without VID/PID filtering, or if your unit reports a non-default VID/PID.

`qrng_close()` closes the handle and frees it. Safe to call with `NULL`. Do not use the handle afterwards.

**The device begins streaming random data the moment it is opened** — there is no start command and no handshake. Data produced while you are not reading is discarded by the device; this does not bias the output.

**Only one process may hold a device open at a time** (standard serial-port semantics). A second `qrng_open()` on the same device returns `QRNG_E_OPEN`.

```c
qrng_device_t *dev = NULL;
qrng_result_t r = qrng_open("/dev/ttyACM0", &dev);
if (r != QRNG_OK) { /* handle */ }
/* ... */
qrng_close(dev);
```

---

### 4.5 Reading random data

```c
qrng_ssize_t  qrng_read(qrng_device_t *dev, void *buf, size_t len);
qrng_result_t qrng_read_full(qrng_device_t *dev, void *buf, size_t len);
void          qrng_set_timeout(qrng_device_t *dev, uint32_t timeout_ms);
```

**`qrng_read()`** reads *up to* `len` bytes and returns how many it got. The return value is a signed size:

| Return | Meaning |
|---|---|
| `> 0` | Number of bytes written into `buf` — **may be less than `len`** |
| `0` | Timeout elapsed with no data. **Not** an error and **not** end-of-stream |
| `< 0` | A `qrng_result_t` error code, cast to the signed type |

Always test for `< 0` before using the value as a length:

```c
unsigned char buf[4096];
qrng_ssize_t n = qrng_read(dev, buf, sizeof buf);
if (n < 0)       { fprintf(stderr, "%s\n", qrng_strerror((qrng_result_t)n)); }
else if (n == 0) { /* timed out — retry */ }
else             { consume(buf, (size_t)n); }
```

**`qrng_read_full()`** loops until exactly `len` bytes have been read. Returns `QRNG_OK`, or `QRNG_E_TIMEOUT` if any single underlying read times out before completing. Use this when you need a fixed-size quantity such as a key.

**`qrng_set_timeout()`** sets the per-read timeout in milliseconds (default **5000**). It applies to `qrng_read()`, to each internal read inside `qrng_read_full()`, and as the overall deadline for control commands. A value of 0 makes reads effectively non-blocking (poll style).

**Throughput.** The device delivers roughly **90–125 KB/s** (750 kbps – 1 Mbps). This is the device's conditioned-entropy rate, not a library limit. Read in chunks of a few KB; single-byte reads waste syscalls. For a fixed small secret, `qrng_read_full()` returns in well under a millisecond once the stream is flowing.

---

### 4.6 Device status and control

```c
qrng_result_t qrng_get_firmware_version(qrng_device_t *dev,
                                        char *out, size_t out_max);
```

Queries the device and writes its firmware version (e.g. `"1.0.0"`) as a NUL-terminated string. A 32-byte buffer is ample.

```c
typedef struct {
    uint64_t total_samples;      /* ADC samples processed since power-on   */
    uint32_t rct_failures;       /* deprecated — use health_failures       */
    uint32_t apt_failures;       /* deprecated — always 0                  */
    int      alarm;              /* 1 while the pipeline is paused         */
    uint64_t conditioned_bytes;  /* random bytes emitted since power-on    */
    uint32_t health_failures;    /* combined RCT + APT trips since power-on*/
    int      running;            /* 1 if the entropy pipeline is active    */
} qrng_health_status_t;

qrng_result_t qrng_get_health(qrng_device_t *dev, qrng_health_status_t *out);
```

Reads the device's NIST SP 800-90B health counters. This is the call to poll in a long-running integration — every few seconds is plenty.

Interpreting the values:

- **`running == 1`, `health_failures` stable** — healthy. Normal steady state.
- **`health_failures` incrementing rarely** (hours or days apart) — expected statistical false positives. The device pauses briefly and recovers on its own. No action needed.
- **`health_failures` climbing steadily, or `running == 0` persistently** — the noise source has degraded or failed. Stop using the output for security purposes and refer to the Troubleshooting Guide.

A healthy unit accumulates fewer than one trip per 10⁹ samples.

```c
qrng_health_status_t st;
if (qrng_get_health(dev, &st) == QRNG_OK && (!st.running || st.health_failures > threshold))
    raise_alert();
```

```c
qrng_result_t qrng_rekey(qrng_device_t *dev);
```

Forces an immediate AES conditioner rekey. The device rekeys automatically every 16 MiB, so this is rarely needed — it exists for policies that require an explicit key rotation at a known point.

```c
qrng_result_t qrng_enter_dfu(qrng_device_t *dev);
```

Reboots the device into the USB DFU bootloader for firmware updates. The device disconnects immediately; **the handle is invalid after this call** — do not use it again (do not even call `qrng_close()` on it, the call already released it). See the Firmware Update Guide.

```c
qrng_result_t qrng_control(qrng_device_t *dev, const char *cmd,
                           char *reply, size_t reply_max);
```

Low-level escape hatch: sends `cmd` (without a trailing newline — one is appended) and returns the reply. The convenience wrappers above are preferred. Pass `reply = NULL` to send without waiting for a reply.

**Why this is not a plain write-then-read:** the device streams random bytes continuously and injects command replies *into that same stream*. A naive "send command, read next line" returns random data, because a random byte is `\n` roughly once every 256 bytes. `qrng_control()` therefore scans the incoming stream for each command's distinctive reply signature and discards the random bytes it passes over. Losing those bytes is harmless — the stream is ergodic.

Commands with recognised signatures: `VERSION`, `STATUS`, `REKEY`, `DFU`. Any other command falls back to a heuristic "printable line" match, which can occasionally match random data — treat custom commands as best-effort.

---

## 5. Threading

The library is **not thread-safe per handle.** A `qrng_device_t *` must be used by one thread at a time.

- **Safe:** separate threads each holding their own handle to *different* devices.
- **Safe:** one thread owning a handle, other threads receiving data through your own queue.
- **Unsafe:** two threads calling `qrng_read()` on the same handle.
- **Unsafe:** one thread in `qrng_read()` while another calls `qrng_get_health()` on the same handle — the reader will consume the reply and the status call will time out.

The recommended pattern for a service is a single owner thread that reads the stream and periodically calls `qrng_get_health()` between reads, publishing both to the rest of the application.

There is no global state, so distinct handles in distinct threads need no locking.

---

## 6. Linux Permissions

By default `/dev/ttyACM*` is owned by `root:dialout`, so `qrng_open()` returns `QRNG_E_OPEN` for ordinary users. Two fixes:

**Add the user to `dialout`** (simplest):

```bash
sudo usermod -aG dialout $USER      # log out and back in to take effect
```

**Or install a udev rule** (better for services and appliances):

```bash
sudo cp tools/rngd-bridge/99-quantarng.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

The rule matches the device by VID/PID and grants access without group membership, and gives a stable symlink so you are not exposed to `ttyACM` renumbering.

On Raspberry Pi OS the default user is already in `dialout`; a fresh Debian Bookworm install is not.

---

## 7. Platform Notes

**Linux** — enumeration resolves USB VID/PID through `/sys/class/tty/<name>/device`, walking up to the parent USB device. Devices whose IDs cannot be read are skipped rather than guessed at. `ttyACM` numbering is not stable across reboots or replugs; use the udev symlink or match on the serial number from `qrng_enumerate()` if you need to pin a specific unit.

**ARM (aarch64 / armhf)** — no architecture-specific code; the library is endian-neutral and has no unaligned-access assumptions. Both are built and published for every release.

**macOS / FreeBSD** — no VID/PID filtering (that would require IOKit / libusb, which the library deliberately avoids). Every CDC-ACM port is reported. Prefer an explicit path when other CDC devices may be attached.

**Windows** — enumeration uses SetupAPI with a VID/PID hardware-ID match. Paths are returned in `\\.\COMn` form, which is required for COM ports above 9.

---

## 8. Error Handling Patterns

**Device unplugged mid-stream.** `qrng_read()` returns `QRNG_E_IO`. Close the handle, then re-enumerate and reopen with backoff — the path may change on reconnect:

```c
qrng_ssize_t n = qrng_read(dev, buf, sizeof buf);
if (n == QRNG_E_IO) {
    qrng_close(dev);
    dev = NULL;
    /* retry qrng_open() with backoff; the path may have changed */
}
```

**Timeouts are not failures.** `qrng_read()` returning 0 means no data arrived within the window. Retry. Persistent zeros with a connected device mean the pipeline is paused — check `qrng_get_health()`.

**Never treat 0 as end-of-stream.** The stream is infinite; there is no EOF.

**Do not mix random data and error codes.** Because errors share the return channel with the byte count, always branch on `< 0` first.

---

## 9. Security Guidance

- **Mix with the OS RNG for long-lived secrets.** For master keys and signing keys, combine QuantaRNG output with `getrandom(2)` / `BCryptGenRandom` rather than relying on either source alone — e.g. hash the concatenation. Both sources would have to fail simultaneously to weaken the result. This is standard practice for any external entropy source and costs nothing.
- **Monitor `qrng_get_health()`** in production and alert on `running == 0` or a rising `health_failures`.
- **Zeroise buffers** holding key material after use; the library does not clear caller-supplied buffers.
- The device's output is already conditioned and full-entropy — do not "whiten" it further, and do not use it to seed a weaker userspace PRNG when you can read from it directly.

---

## 10. Examples in the Repository

| File | Description |
|---|---|
| `examples/basic.c` | Read 4 KiB and print it |
| `examples/stream.c` | Continuous read with throughput statistics |
| `examples/info.c` | Enumerate devices, print firmware and health |

Built by default (`-DBUILD_EXAMPLES=ON`) as `qrng-basic`, `qrng-stream`, `qrng-info`.

---

## 11. Support

| Need | Contact |
|---|---|
| Integration questions, bug reports | support@dyber-pqc.com |
| Security reports | security@dyber-pqc.com |
| Source, releases, issues | github.com/dyber-pqc/QuantaRNG-USB-Pub |

When reporting an SDK issue, include: `qrng_version()`, device firmware version, OS and architecture (`uname -a`), and the failing call with its `qrng_strerror()` output.

---

*© 2026 Dyber, Inc. — QuantaRNG USB C SDK API Reference (SDK 1.1.0)*
