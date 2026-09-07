# QuantaRNG C SDK (`libquantarng`)

Cross-platform C library for the QuantaRNG USB device.

**📖 Full API reference: [docs/guides/c-sdk-api.md](../../docs/c-sdk-api.md)** — function-by-function
semantics, error handling, threading rules, and ARM cross-compilation.

| Platform | Device path | Enumeration |
|---|---|---|
| Linux (x86_64, aarch64, armhf) | `/dev/ttyACM*` | USB VID/PID via sysfs |
| macOS (Apple Silicon, Intel) | `/dev/cu.usbmodem*` | Path glob (no VID/PID filter) |
| Windows 10/11 (x64) | `\\.\COM*` | USB VID/PID via SetupAPI |
| FreeBSD 13+ | `/dev/cuaU*` | Path glob (no VID/PID filter) |

Prebuilt binaries for all of the above — including **aarch64** and **armhf** — are
attached to every [release](https://github.com/dyber-pqc/QuantaRNG-USB-Pub/releases/latest).

## Building

### Linux / macOS / FreeBSD

```bash
cd sdk/c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

Installs:
- `/usr/local/lib/libquantarng.so.1.0.0` (symlinks: `.so.1`, `.so`)
- `/usr/local/include/quantarng.h`
- `/usr/local/lib/pkgconfig/quantarng.pc`

### Windows

```powershell
cd sdk\c
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
cmake --install build --config Release --prefix C:\quantarng
```

## Usage

```c
#include <quantarng.h>

qrng_device_t *dev = NULL;
if (qrng_open(NULL, &dev) != QRNG_OK) { exit(1); }

uint8_t buf[1024];
qrng_read_full(dev, buf, sizeof(buf));

qrng_close(dev);
```

Compile:

```bash
gcc app.c $(pkg-config --cflags --libs quantarng) -o app
```

## API reference

See `include/quantarng.h` for full Doxygen-annotated declarations.

| Function | Purpose |
|---|---|
| `qrng_version()` | Returns SDK version string |
| `qrng_enumerate()` | List attached devices |
| `qrng_open()` | Open by path or first device |
| `qrng_close()` | Close handle |
| `qrng_read()` | Read up to N bytes (blocking with timeout) |
| `qrng_read_full()` | Read exactly N bytes |
| `qrng_set_timeout()` | Set per-read timeout (ms) |
| `qrng_control()` | Send command, read reply |
| `qrng_get_firmware_version()` | Convenience: device firmware version |
| `qrng_get_health()` | Convenience: NIST 800-90B health status |
| `qrng_rekey()` | Trigger AES rekey |
| `qrng_enter_dfu()` | Reboot into bootloader |
| `qrng_strerror()` | Translate error codes |

## Examples

| File | Description |
|---|---|
| `examples/basic.c` | Read 4 KiB and print |
| `examples/stream.c` | Continuous read with throughput stats |
| `examples/info.c` | Enumerate devices and print health |

Build all examples with `-DBUILD_EXAMPLES=ON` (default).

## Permissions (Linux)

Out of the box, only root can open `/dev/ttyACM*`. To allow regular users:

```bash
sudo usermod -aG dialout $USER     # log out and back in
```

Or install the udev rule:

```bash
sudo cp tools/rngd-bridge/99-quantarng.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Cross-compiling for ARM

```bash
sudo apt-get install gcc-aarch64-linux-gnu          # arm64
cmake -B build-arm64 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
cmake --build build-arm64 -j
```

Use `gcc-arm-linux-gnueabihf` / `arm-linux-gnueabihf-gcc` and
`-DCMAKE_SYSTEM_PROCESSOR=arm` for 32-bit (armhf) targets. See the
[API reference](../../docs/c-sdk-api.md#32-cross-compiling-for-arm-raspberry-pi-and-similar)
for choosing between them.

## Tests

```bash
cmake -B build -DBUILD_TESTS=ON && cmake --build build -j
ctest --test-dir build --output-on-failure
```

The suite covers reply extraction from the interleaved random stream and
needs no hardware.

## Threading

The library is **not** thread-safe per handle. Each thread should hold
its own `qrng_device_t *`. In particular, do not call `qrng_read()` from
one thread while another calls `qrng_get_health()` on the same handle —
the reader will consume the reply.

Only one **process** may hold a given device open at a time; the OS
enforces exclusive access to the serial port.

## License

MIT (see [LICENSE](../../LICENSE)).
