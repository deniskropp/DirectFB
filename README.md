# DirectFB 1.x — Canonical Maintenance Repository

**DirectFB** is a graphics library designed for embedded systems. It delivers maximum hardware-accelerated performance with minimal resource usage and overhead.

> **2026 Status**: This is the primary maintained repository for the original **DirectFB 1.x** series, stewarded by original author **Denis Oliver Kropp** (dok@directfb.org).

The original `directfb.org` site is no longer active. This GitHub repository (together with the companion `linux-fusion` module) serves as the authoritative source for DirectFB 1.x.

A community-driven continuation/revival project exists at **[DirectFB2](https://github.com/directfb2/DirectFB2)**.

## Development Direction

See the living roadmap and architectural analysis:

- [MODERNIZATION-ROADMAP.md](MODERNIZATION-ROADMAP.md)
- [docs/TAS-EXTRACTION.md](docs/TAS-EXTRACTION.md)

Contributions aligned with the roadmap (especially Phase 0–3) are welcome.

---

## Original README (Historical & Technical Reference)

DirectFB README
---------------

DirectFB is a graphics library which was designed with embedded
systems in mind. It offers maximum hardware accelerated performance
at a minimum of resource usage and overhead. 

Check the GitHub repository and DirectFB2 for current information.

### Supported Operating Systems

- **GNU/Linux** (primary, with fbdev)

Using SDL (experimental, without acceleration), DirectFB also supports:

- FreeBSD
- NetBSD
- OpenBSD

Native support for other platforms (e.g. macOS) is limited.

### Build Requirements

**Mandatory**:
- libc, libpthread, libm, libdl

**For regenerating build system** (`./autogen.sh`):
- autoconf, automake, libtool, pkg-config

**Recommended optional packages** (Debian names):
- libfreetype6-dev (fonts)
- libjpeg-dev, libpng-dev (images)
- zlib1g-dev

For the multi-application core: `linux-fusion` kernel module (see below).

### Usage Requirements

- Working framebuffer device (`/dev/fb0`)
- Keyboard and mouse access
- For multi-app: `linux-fusion` module + tmpfs on `/dev/shm`

See the full original instructions below for detailed configuration, multi-application setup, and installation steps.

### Running multiple DirectFB applications

Enable with `./configure --enable-multi` and install the `linux-fusion` kernel module (companion repo: https://github.com/deniskropp/linux-fusion).

### Installation

```bash
./autogen.sh
./configure [options]
make
sudo make install
```

Use `./configure --help` for options (`--enable-multi`, `--enable-debug`, etc.).

After installation, ensure the library path is configured (`ldconfig`).

### Hardware Acceleration

Historically optimized for Matrox cards. Other drivers (ATI, etc.) provide partial acceleration.

### Further Reading

- Full technical details are preserved in the original sections below.
- For the current evolution plan, see the **Modernization Roadmap** linked above.

---

## Contributing

Please see [CONTRIBUTING.md](CONTRIBUTING.md) and the [Modernization Roadmap](MODERNIZATION-ROADMAP.md).

**License**: LGPL-2.0-or-later

**Maintained by**: Denis Oliver Kropp

*This repository preserves 25+ years of embedded graphics heritage while preparing DirectFB for modern AI-augmented and embedded vision use cases.*