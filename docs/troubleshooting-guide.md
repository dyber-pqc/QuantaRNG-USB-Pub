# QuantaRNG USB — Troubleshooting Guide

**Solutions to common issues, from setup to firmware recovery**

*Dyber, Inc. — Customer Documentation*
*Revision 1.0 — September 2026 — applies to firmware 1.0.x and host software 0.1.x*

---

## 1. Before You Start

Most issues resolve with three quick checks:

1. **Plug the device directly into the computer** — some unpowered USB hubs and front-panel ports deliver marginal power.
2. **Close other programs that may hold the device** — only one program can open QuantaRNG at a time (it is a serial device). Close terminal programs, other capture tools, or a second copy of the desktop app.
3. **Check the LED** — a steady green LED means the device booted and the entropy pipeline is running.

---

## 2. Device Not Detected

### Windows

**Symptom:** the device does not appear in the QuantaRNG USB app or in Device Manager under "Ports (COM & LPT)."

1. Open Device Manager → **Ports (COM & LPT)**. Look for a COM port that appears when you plug the device in and disappears when you unplug it.
2. If it appears under **"Other devices"** with a warning icon: Windows 10 and 11 include the CDC driver natively — right-click → *Update driver* → *Search automatically*.
3. Try a different USB port (prefer a rear motherboard port on desktops).
4. In the QuantaRNG USB desktop app, click **Refresh**. If the device still isn't listed but you can see its COM port in Device Manager, type the port name (e.g. `COM7`) directly into the device box and click Connect.

### Linux

1. Check enumeration: `ls /dev/ttyACM*` — the device should appear as `/dev/ttyACM0` (or higher).
2. Check the kernel log: `dmesg | tail` should show a new CDC ACM device with ID `0483:5740`.
3. **Permission denied** opening the port: add yourself to the `dialout` group (`sudo usermod -aG dialout $USER`, then log out and back in), or install the udev rule shipped with `quantarng-rngd`.

### macOS

1. Check: `ls /dev/cu.usbmodem*`
2. No entry: try another port or cable, and check *System Information → USB* for "QuantaRNG USB."

---

## 3. Connected but No Data / Slow Data

**Symptom:** the port opens but reads return nothing, or throughput is far below ~90 KB/s.

- **Another process holds the device.** Only one reader is allowed. On Linux: `sudo lsof /dev/ttyACM0`. On Windows: close other terminal apps; if unsure, unplug and replug the device.
- **Pipeline paused.** Query the device status (desktop app health panel, `quantarng info`, or send `STATUS`). If `running` is 0, the built-in health tests have paused output — see §4.
- **Throttled USB path.** Long extension cables and daisy-chained hubs can degrade Full-Speed USB. Connect directly.

Normal throughput is **roughly 90–125 KB/s** (750 kbps – 1 Mbps). This is limited by the device's conditioned entropy rate, not your computer.

---

## 4. Health-Test Warnings

QuantaRNG continuously runs the NIST SP 800-90B health tests (Repetition Count Test and Adaptive Proportion Test) on the raw noise source. The `hf` counter in `STATUS` (shown as "Health failures" in the desktop app) counts test trips since power-on.

| Observation | Meaning | Action |
|---|---|---|
| `hf` = 0 | Healthy | None |
| `hf` increments rarely (hours/days apart) | Expected statistical false positives | None — output pauses briefly and auto-recovers |
| `hf` climbs steadily; output frequently paused | Noise source degradation | See below |
| `running` = 0 permanently | Source failure detected | See below |

**If failures climb steadily or the pipeline stays paused:**

1. Unplug the device, wait 10 seconds, and replug (full power cycle resets the analog bias).
2. Try a different USB port — marginal supply voltage can starve the internal bias supply.
3. If the condition persists across ports and computers, the noise source may have degraded. Stop using the device for security-critical purposes and contact support with your serial number and a screenshot of the health panel.

---

## 5. Windows Desktop App Issues

**"No QuantaRNG devices found"** — see §2 (Windows). The manual COM-port entry is the universal fallback.

**"Connect failed: Could not open COMx: Access is denied"** — another program has the port open, or a previous connection didn't close cleanly. Close other software, unplug/replug the device, then Connect again.

**App shows SmartScreen warning at install** — if Windows shows "Windows protected your PC," click *More info* and confirm the publisher reads **Dyber, Inc.** before choosing *Run anyway*. Our installers are always Authenticode-signed; do not run an installer that shows "Unknown publisher."

**Capture file is empty or tiny** — capture writes only while the device is connected and streaming. Check the throughput display is non-zero, then start capture.

**Stats look "off" (ones fraction far from 0.5000)** — over short windows small deviations are normal. The displayed statistics use a ~1 MB rolling window; sustained deviation beyond ±0.002 with a healthy device is unexpected — contact support.

---

## 6. Firmware Update Problems

**Device won't enter DFU mode**

- Software method: send `DFU` (or use the desktop app's *Enter DFU bootloader* button). The device disconnects and re-enumerates as a DFU device.
- Hardware method (always works): unplug, **hold the SW1 button**, plug in while holding, release after one second.
- Verify: `dfu-util -l` should list `[0483:df11]`. On Windows, STM32CubeProgrammer's USB list should show the device.

**"DFU device not found" on Windows** — install the WinUSB driver for the DFU interface once, using Zadig (zadig.akeo.ie).

**"Cannot open DFU device" / flashing fails midway** — plug directly into the computer (not a hub) and use a known-good cable. Then re-run the flash command.

**Device seems dead after a failed update** — it isn't. The DFU bootloader is in read-only ROM and cannot be corrupted. Use the hardware method (SW1 held while plugging in) and flash again.

**Device runs the old version after flashing** — include `:leave` in the dfu-util command (`-s 0x08000000:leave`), or simply unplug and replug.

Always verify downloaded firmware against the published SHA-256 checksum and GPG signature before flashing.

---

## 7. Validating Randomness Quality

If you want to verify output quality yourself:

```bash
quantarng dump --bytes 75000000 -o sample.bin
dieharder -a -g 201 -f sample.bin
```

Or capture with the Windows desktop app and run the NIST scripts from the repository (`tools/entropy-tests/`). Expected: all Dieharder default tests pass; NIST IID min-entropy ≥ 0.85 bits/bit on raw captures.

A single "weak" result in a large Dieharder run is statistically normal; consistent failures are not — contact support with the sample file if you can share it.

---

## 8. Contacting Support

Include the following for fastest resolution:

1. **Serial number** (printed on the device; also shown in the desktop app)
2. **Firmware version** (`quantarng info` or the app's Device panel)
3. **Host OS and software version**
4. **STATUS output or a screenshot** of the desktop app health panel
5. What you tried from this guide

| Channel | Address |
|---|---|
| General support | support@dyber.org |
| Security issues | security@dyber.org |
| Releases & firmware | github.com/dyber-pqc/QuantaRNG-USB/releases |

*Report suspected security vulnerabilities privately to security@dyber.org — please do not open public issues for security matters.*

---

*© 2026 Dyber, Inc. — QuantaRNG USB Troubleshooting Guide*
