# QuantaRNG USB — Firmware Update Guide

**Updating your device safely over USB — no special hardware required**

*Dyber, Inc. — Customer Documentation*
*Revision 1.0 — September 2026 — applies to all QuantaRNG USB devices*

---

## 1. About Firmware Updates

Your QuantaRNG USB contains upgradable firmware that runs the entropy pipeline — sampling, health testing, cryptographic conditioning, and USB streaming. Dyber publishes firmware updates to add features, refine performance, and address any reported issues.

Updates use the **USB DFU (Device Firmware Upgrade)** standard, built into the device's read-only ROM. Two important consequences:

- **You only need a USB cable.** No programmer, no opening the case.
- **The device cannot be bricked.** The bootloader lives in read-only memory; even a failed or interrupted update is always recoverable.

### Checking your current version

- **Windows desktop app** — shown in the Device panel after connecting.
- **CLI** — `quantarng info`
- **Any terminal program** — send `VERSION`; the device replies `QuantaRNG <version>`.

---

## 2. Downloading Firmware

Official firmware is published at **github.com/dyber-pqc/QuantaRNG-USB/releases**. Each release includes:

| File | Purpose |
|---|---|
| `quantarng-fw-<version>.bin` | Firmware image for `dfu-util` |
| `quantarng-fw-<version>.hex` | Image for STM32CubeProgrammer |
| `.sha256` files | Integrity checksums |
| `.sig` files | GPG signatures |

**Verify before flashing** (strongly recommended):

```bash
sha256sum -c quantarng-fw-1.0.0.bin.sha256
gpg --verify quantarng-fw-1.0.0.bin.sig quantarng-fw-1.0.0.bin
```

The signing key fingerprint is published at dyber-pqc.com/keys. Only flash firmware that passes both checks.

---

## 3. Entering DFU Mode

The device must be in DFU mode to accept an update. Two ways:

### Method A — from software (device working normally)

- **Windows desktop app**: connect, then click **Enter DFU bootloader…** and confirm.
- **Python**: 
  ```python
  from quantarng import QuantaRNG
  with QuantaRNG.open() as dev:
      dev.enter_dfu()
  ```
- **Any serial terminal**: send `DFU`.

The device disconnects and re-enumerates as a DFU device within a few seconds.

### Method B — the button (always works, even with broken firmware)

1. Unplug the device.
2. **Hold the SW1 button** on the device.
3. Plug it in while holding; keep holding for one second, then release.

### Confirming DFU mode

```bash
dfu-util -l
```

You should see a line containing `[0483:df11]`. On Windows, STM32CubeProgrammer's USB device list serves the same purpose.

> **Windows, first time only:** if the DFU device isn't detected, install the WinUSB driver for it using Zadig (zadig.akeo.ie) — select the DFU device, choose WinUSB, click Install.

---

## 4. Flashing

### Linux / macOS / FreeBSD (dfu-util)

Install the tool (`apt install dfu-util`, `brew install dfu-util`, or `pkg install dfu-util`), then:

```bash
sudo dfu-util -a 0 -s 0x08000000:leave -D quantarng-fw-1.0.0.bin
```

The `:leave` suffix makes the device reboot into the new firmware automatically when flashing completes.

### Windows (STM32CubeProgrammer)

Install STM32CubeProgrammer (free from st.com), then:

```powershell
STM32_Programmer_CLI.exe -c port=USB1 -d quantarng-fw-1.0.0.hex -v -g
```

`-v` verifies after writing; `-g` starts the new firmware. The dfu-util Windows binary works as an alternative.

### What you'll see

Flashing takes a few seconds. The device then reboots, re-enumerates as a normal serial device, and the LED returns to steady green.

---

## 5. Verifying the Update

Reconnect with any tool and confirm the version:

```bash
quantarng info
```

or connect with the Windows desktop app — the Device panel shows the new firmware version. Give the device a few seconds after replug; it streams entropy within half a second of enumeration.

We recommend a quick health check after updating: the app's health panel should show **RUNNING** with 0 failures accumulating.

---

## 6. Troubleshooting Updates

| Problem | Fix |
|---|---|
| `dfu-util -l` shows nothing | Re-enter DFU mode (use Method B); try a direct USB port; on Windows install WinUSB via Zadig |
| "Cannot open DFU device" | Run with `sudo` (Linux), or fix permissions; avoid hubs and long cables |
| Flash fails partway | Swap cable/port and simply re-run — interrupted flashes are harmless |
| Device runs the old version afterward | Use `:leave` in the dfu-util command, or unplug/replug |
| Verification error repeats on good cable | Contact support — do not continue using the unit |
| Device "seems dead" after failed flash | It isn't — use Method B (button) and flash again; ROM DFU is unbrickable |

---

## 7. Rolling Back

All previous firmware releases remain available on the releases page. To roll back, flash the older version using the exact same procedure. Check the release notes for any compatibility notes before downgrading.

---

## 8. Support

If an update problem persists after §6:

- **support@dyber-pqc.com** — include your serial number, current firmware version, host OS, the exact command used, and its full output.

---

*© 2026 Dyber, Inc. — QuantaRNG USB Firmware Update Guide*
