# QuantaRNG USB

**True hardware random number generator — quantum shot noise, NIST SP 800-90B health-tested, AES-conditioned, streamed over USB.**

QuantaRNG USB is a compact USB-A device by [Dyber, Inc.](https://dyber-pqc.com) that generates cryptographically secure random numbers from a quantum-mechanical noise source. It appears as a standard serial port on Windows, macOS, Linux, and FreeBSD — **no drivers required**. Plug it in and read random bytes at up to 1 Mbps of full-entropy output.

This repository hosts the **official software downloads and end-user documentation**.

---

## 📥 Downloads

Get the latest release from the **[Releases page](https://github.com/dyber-pqc/QuantaRNG-USB-Pub/releases/latest)**.

| Platform | What to download |
|---|---|
| **Windows 10/11** | `QuantaRNG-Setup-<version>.exe` — signed installer with the desktop dashboard app |
| **Windows (portable)** | `QuantaRNG.exe` — signed standalone app, no installation |
| **Linux / macOS / FreeBSD** | `pip install quantarng` — CLI + Python SDK ([PyPI](https://pypi.org/project/quantarng/)) |
| **C developers** | `libquantarng-<platform>.tar.gz` — prebuilt native libraries, or build from [`sdk/c/`](sdk/c) |
| **Raspberry Pi / ARM64** | `libquantarng-linux-aarch64.tar.gz` (64-bit) |
| **Raspberry Pi / 32-bit ARM** | `libquantarng-linux-armhf.tar.gz` (`uname -m` = `armv7l`) |

All Windows binaries are Authenticode-signed by **"Dyber, Inc."** — check the digital-signature tab before running anything claiming to be QuantaRNG software.

### Quick start

**Windows** — run the installer, launch *QuantaRNG USB*, plug in the device, click **Connect**. You get live health monitoring, throughput stats, and one-click entropy capture.

**Linux / macOS**

```bash
pip install quantarng
quantarng info                                 # device + health summary
quantarng dump --bytes 1048576 -o random.bin   # capture 1 MiB
```

**Linux system-wide entropy** — feed the kernel pool for every application:

```bash
sudo systemctl enable --now quantarng-rngd
```

---

## 🛠 C SDK — build for any target

The full C SDK source lives in **[`sdk/c/`](sdk/c)** (MIT). It is C11 with **no external
dependencies** beyond libc, so it builds anywhere with a C compiler — including
ARM SBCs, embedded Linux, and BSDs.

```bash
cd sdk/c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

Cross-compiling for a Raspberry Pi or similar (arm64 shown; see the
[API reference](docs/c-sdk-api.md#32-cross-compiling-for-arm-raspberry-pi-and-similar)
for armhf and for choosing between them):

```bash
sudo apt-get install gcc-aarch64-linux-gnu
cmake -B build-arm64 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
cmake --build build-arm64 -j
```

```c
#include <quantarng.h>

qrng_device_t *dev;
qrng_open(NULL, &dev);              /* first device found */

unsigned char key[32];
qrng_read_full(dev, key, sizeof key);

qrng_close(dev);
```

Hardware-free unit tests: `cmake -B build -DBUILD_TESTS=ON && ctest --test-dir build`.

## 📚 Documentation

| Guide | Contents |
|---|---|
| [**C SDK API Reference**](docs/c-sdk-api.md) | Every function, error semantics, threading, permissions, cross-compiling |
| [Software Guide](docs/software-guide.md) | Desktop app, CLI, Python / C / .NET SDKs, Linux daemon |
| [Troubleshooting Guide](docs/troubleshooting-guide.md) | Detection issues, health warnings, recovery — per OS |
| [Firmware Update Guide](docs/firmware-update-guide.md) | Safe USB (DFU) updates, verification, rollback |

PDF versions of each guide are attached to every [release](https://github.com/dyber-pqc/QuantaRNG-USB-Pub/releases/latest).

---

## ✨ About the device

- **Quantum entropy source** — shot noise from avalanche breakdown in a reverse-biased semiconductor junction
- **Continuous NIST SP 800-90B health tests** (Repetition Count + Adaptive Proportion) on every raw sample
- **AES-128-CBC-MAC conditioning** (NIST-vetted construction) in hardware, forward-secure rekeying
- **750 kbps – 1 Mbps** conditioned full-entropy output, host-throttled over USB CDC
- **Per-unit certification** — every device ships with a signed NIST min-entropy report, retrievable by serial number
- Passes **Dieharder, NIST STS, TestU01, PractRand** — and you can [re-run the tests yourself](docs/software-guide.md)
- **Unbrickable firmware updates** via the read-only ROM USB DFU bootloader

---

## 🔐 Verifying downloads

- **Windows binaries**: right-click → Properties → Digital Signatures → publisher must read **Dyber, Inc.**
- **Firmware images**: each ships with SHA-256 checksums and a GPG signature; the signing key fingerprint is published at [dyber-pqc.com/keys](https://dyber-pqc.com/keys).

```bash
sha256sum -c quantarng-fw-<version>.bin.sha256
gpg --verify quantarng-fw-<version>.bin.sig quantarng-fw-<version>.bin
```

---

## 🆘 Support

- Start with the [Troubleshooting Guide](docs/troubleshooting-guide.md)
- **support@dyber.org** — include your device serial number and firmware version
- **security@dyber.org** — for security reports (see [SECURITY.md](SECURITY.md); please don't open public issues for vulnerabilities)

---

## 📄 License

Software in this repository and in the releases is licensed under the [MIT License](LICENSE).

---

**Made by [Dyber, Inc.](https://dyber-pqc.com)** — post-quantum cryptography, classical entropy, real silicon.
