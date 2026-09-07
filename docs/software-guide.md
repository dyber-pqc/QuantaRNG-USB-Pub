# QuantaRNG USB — Software Guide

**Installing and using the QuantaRNG host software: desktop app, CLI, and SDKs**

*Dyber, Inc. — Customer Documentation*
*Revision 1.0 — September 2026 — covers host software 0.1.x*

---

## 1. Software Overview

QuantaRNG works out of the box as a plain serial device, but the official software makes it effortless:

| Component | Platform | Best for |
|---|---|---|
| QuantaRNG USB desktop app | Windows 10/11 | Monitoring, capture, firmware updates — no coding |
| `quantarng` CLI + Python SDK | Windows, macOS, Linux, FreeBSD | Scripting, automation, data capture |
| `libquantarng` C SDK | Windows, macOS, Linux, FreeBSD | Native integration into your applications |
| QuantaRng.Core .NET SDK | Windows | .NET / C# applications |
| `quantarng-rngd` | Linux | Feeding the OS entropy pool system-wide |

All software is open source (MIT) at **github.com/dyber-pqc/QuantaRNG-USB**.

---

## 2. Windows Desktop App

### 2.1 Installing

1. Download **QuantaRNG-Setup-\<version\>.exe** from the releases page.
2. Run it. Confirm the publisher shows **Dyber, Inc.** (our installers are always signed).
3. Follow the wizard: accept the license, choose the install location, optionally add a desktop shortcut.
4. Launch **QuantaRNG USB** from the Start menu.

A standalone `QuantaRNG.exe` (no installation) is also published with each release. Neither requires a separate .NET runtime.

### 2.2 Using the app

**Connect** — plug in the device, click *Refresh*, pick the device, click *Connect*. The app shows the port, firmware version, and serial number. If auto-detection misses your device, type its COM port directly.

**Health panel** — live NIST SP 800-90B status polled every 2 seconds:

| Field | Meaning |
|---|---|
| Pipeline | RUNNING (green) or PAUSED (red) |
| ADC samples | Raw samples processed since power-on |
| Bytes emitted | Conditioned random bytes produced since power-on |
| Health failures | Health-test trips (0 on a healthy device; see the Troubleshooting Guide) |

**Stream monitor** — live throughput, total bytes received, a rolling bit-balance ("ones fraction," expect ≈ 0.5000) and mean byte value (expect ≈ 127.5), plus a live hex preview of the stream.

**Capture to file** — choose a destination, click *Start capture*, and the raw random stream is written to disk until you stop. Files are suitable for direct use or for entropy test suites (Dieharder, NIST).

**Actions** — *Rekey conditioner* forces an immediate AES rekey. *Enter DFU bootloader* reboots the device for firmware updates (with confirmation; see the Firmware Update Guide).

---

## 3. Python SDK & Command-Line Tool

### 3.1 Install

```bash
pip install quantarng
```

Python 3.9+; Windows, macOS, Linux, FreeBSD.

### 3.2 Command-line usage

```bash
quantarng info                                # device, firmware, health summary
quantarng dump --bytes 1048576 -o random.bin  # capture 1 MiB to a file
quantarng stream                              # stream bytes to stdout
```

Examples:

```bash
# Generate a 256-bit key in hex
quantarng dump --bytes 32 | xxd -p -c 32

# Feed a test suite directly
quantarng stream | dieharder -a -g 200
```

### 3.3 Python API

```python
from quantarng import QuantaRNG

# List devices
for dev in QuantaRNG.enumerate():
    print(dev.path, dev.serial_number)

# Read random bytes
with QuantaRNG.open() as dev:
    print("firmware:", dev.firmware_version())
    data = dev.read(4096)              # exactly 4096 bytes, blocking

    health = dev.health()
    print("failures:", health.health_failures)

    for chunk in dev.stream():         # continuous chunks
        process(chunk)
```

Key classes: `QuantaRNG` (device handle), `DeviceInfo` (enumeration metadata), `HealthStatus` (STATUS snapshot). Errors raise `QuantaRNGError` subclasses, e.g. `DeviceNotFoundError`.

---

## 4. C SDK (`libquantarng`)

### 4.1 Build & install

```bash
cd sdk/c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Works with GCC, Clang, and MSVC. `pkg-config quantarng` is provided on Unix-like systems.

### 4.2 API essentials

```c
#include <quantarng.h>

qrng_handle_t *dev = qrng_open(NULL);        /* NULL = first device found */
if (!dev) { /* no device */ }

uint8_t buf[4096];
size_t got = qrng_read(dev, buf, sizeof buf); /* blocking read */

qrng_close(dev);
```

Compile with `-lquantarng`. See `sdk/c/examples/` in the repository for enumeration, streaming, and status examples.

---

## 5. .NET SDK (Windows)

The `QuantaRng.Core` library powers the desktop app and is usable directly from any .NET 10+ project:

```csharp
using QuantaRng.Core;

// Enumerate by USB VID/PID
foreach (var info in DeviceLocator.Enumerate())
    Console.WriteLine(info);

// Open, query, stream
using var device = QuantaRngDevice.OpenFirst();
string fw = await device.GetFirmwareVersionAsync();
HealthStatus health = await device.GetHealthAsync();

device.RandomBytesReceived += chunk =>
{
    // called from a background reader thread with each chunk
};
```

The library handles the device's command/stream interleaving internally — captured entropy is never contaminated by protocol text. Source: `windows/QuantaRng.Core` in the repository.

---

## 6. Linux System Entropy Daemon

To have QuantaRNG feed the kernel entropy pool (`/dev/random`) for all applications:

```bash
sudo systemctl enable --now quantarng-rngd
```

The package installs a udev rule for device permissions and reconnects automatically if the device is unplugged. Verify it's working:

```bash
systemctl status quantarng-rngd
cat /proc/sys/kernel/random/entropy_avail
```

---

## 7. Good Practices

- **One reader per device.** Serial-port semantics allow a single process to hold the device. For multiple consumers, use `quantarng-rngd` (Linux) or a small forwarding service.
- **Mix with the OS RNG for high-stakes keys.** Combine QuantaRNG output with `getrandom()` / `BCryptGenRandom` rather than relying on any single source — standard defense-in-depth for external entropy hardware.
- **Check health counters in long-running deployments.** A `STATUS` poll every few minutes (the desktop app does this automatically) gives early warning of source degradation.
- **Verify signatures.** Windows binaries are Authenticode-signed by "Dyber, Inc."; firmware images carry SHA-256 checksums and GPG signatures.

---

## 8. Resources

| Resource | Location |
|---|---|
| Downloads & releases | github.com/dyber-pqc/QuantaRNG-USB/releases |
| Python package | pypi.org/project/quantarng |
| Source code & examples | github.com/dyber-pqc/QuantaRNG-USB |
| Support | support@dyber.org |

---

*© 2026 Dyber, Inc. — QuantaRNG USB Software Guide*
