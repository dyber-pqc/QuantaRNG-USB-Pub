# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 1.x.y (firmware + SDK) | ✅ |
| 0.x.y | ❌ pre-release |

## Reporting a vulnerability

We take security seriously, especially since QuantaRNG is intended for cryptographic use.

**Please do not report security vulnerabilities through public GitHub issues.**

Instead, email **security@dyber.org** with:

1. A description of the vulnerability
2. Steps to reproduce (or proof-of-concept code)
3. Impact assessment
4. Any suggested fix

Our PGP key is available at [https://dyber-pqc.com/.well-known/pgp-key.txt](https://dyber-pqc.com/.well-known/pgp-key.txt).

### Response timeline

- **Within 48 hours** — initial acknowledgment
- **Within 7 days** — preliminary assessment
- **Within 90 days** — patch released or coordinated disclosure plan agreed

We follow a coordinated disclosure model: once a fix is available, we publish an advisory crediting the reporter (unless anonymity is requested).

## Threat model

### In scope

- Statistical bias in conditioned output
- Timing/side-channel attacks against the AES conditioner
- Physical tampering that compromises entropy quality
- USB protocol attacks against the host
- Firmware update bypass / arbitrary code execution
- Health test bypass or false-pass conditions

### Out of scope

- Attacks requiring physical destruction of the device
- Attacks against the host operating system unrelated to the QuantaRNG
- DoS by unplugging the device
- Issues in the host OS USB stack

## Hardening recommendations for users

- **Verify firmware signatures** before flashing (see [docs/firmware-update.md](docs/firmware-update.md))
- **Run health tests** continuously (the device does this internally; SDK exposes status)
- **Combine with system entropy** (`/dev/random` mixing) rather than as sole source for high-stakes keys
- **Periodically run NIST SP 800-90B IID tests** on captured raw output (every 6–12 months)
- **Tamper detection**: physical inspection — production units have a tamper-evident seal

## Known limitations

- Single noise source (no parallel comparison) — failure of Q1 is detected by health tests but cannot be cross-verified against another source
- USB Full-Speed limits raw throughput to ~12 Mbps line rate (~8 Mbps practical)
- AES-128 is used for conditioning; AES-256 is not exposed but can be enabled in firmware build flags

## CVE history

None reported as of v1.0.0.

---

For non-security questions, see [CONTRIBUTING.md](CONTRIBUTING.md).
