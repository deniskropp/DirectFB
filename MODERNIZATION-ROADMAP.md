# DirectFB 1.x Modernization Roadmap (2026+)

**Maintainer**: Denis Oliver Kropp (deniskropp)  
**Repository**: https://github.com/deniskropp/DirectFB  
**Vision**: Evolve the canonical low-overhead embedded graphics library into a reliable foundation for AI-augmented real-time vision and HMI systems, while preserving its original minimalism and fbdev strengths.

## Strategic Context

- DirectFB 1.x (this repository) remains the authoritative source for the original architecture.
- DirectFB2 exists as a community revival focused on continued embedded relevance.
- Your 25+ years of stewardship positions this work uniquely at the intersection of legacy embedded graphics and modern AI/computer vision.

## Vision

Protect the original philosophy (minimal resource usage, hardware acceleration, deterministic behavior) while adding the capabilities needed for 2026+ embedded AI pipelines:

- Efficient rendering of inference results
- Real-time overlays and HUDs
- Clean integration with modern inference engines
- Strong support for DRM/KMS on current hardware

## Phased Roadmap

### Phase 0 — Foundation Revival (Immediate, 0–4 weeks)
- Update README with current status, DirectFB2 relationship, and authorship credits.
- Add GitHub Actions CI for build validation on multiple architectures.
- Regenerate and publish API documentation.
- Create clear `CONTRIBUTING.md` and licensing notes.
- **Deliverable**: Modern, welcoming landing experience for the repository.

### Phase 1 — Build & Tooling Modernization (4–10 weeks)
- Make CMake the primary/recommended build system (keep autotools for compatibility).
- Update compiler standards and address recent C++ header changes.
- Add static analysis, sanitizers, and basic embedded-focused testing.
- Improve cross-compilation documentation and scripts for common targets (ARM, RISC-V, NVIDIA Jetson, etc.).

### Phase 2 — Backend Evolution (High Priority)
- **Primary Goal**: Add or significantly improve a **DRM/KMS** system module alongside the existing fbdev backend.
- Optional: Lightweight EGL / OpenGL ES translation layer for capable hardware.
- Keep fbdev as the pure, ultra-low-overhead path for constrained devices.
- Align directionally with DirectFB2 efforts (complementary positioning).

### Phase 3 — AI & Computer Vision Integration Layer (Core Opportunity)
- Design a clean **AI Surface Import** extension for efficient (ideally zero-copy) transfer of inference output tensors into DirectFB surfaces.
- Provide optimized primitives for real-time overlay / HUD composition (perfect for defect visualization, monitoring dashboards).
- Support headless/offscreen rendering mode.
- Define performance contracts (predictable latency, bounded memory).
- **Direct synergy**: Ideal companion to autonomous underbody inspection, robotics vision, and other real-time AI pipelines running on embedded Linux.

### Phase 4 — Safety, Testing & Packaging
- Modern test harness focused on embedded constraints and determinism.
- Targeted memory safety improvements in non-hot paths.
- Updated packaging recipes (Alpine, Yocto, Buildroot).
- Clear documentation of Fusion multi-application model on modern kernels.

### Phase 5 — Strategic Positioning & Ecosystem
- Publish positioning statement clarifying relationship between this 1.x maintenance repo and the DirectFB2 revival.
- Create portfolio / case-study artifact highlighting 25+ years of embedded graphics expertise bridging into AI-hybrid systems.
- Explore light collaboration or advisory opportunities with the DirectFB2 project if aligned.

## Prioritization

**High**: Phase 0 + Phase 2 (DRM/KMS) + Phase 3 (AI integration) — maximum portfolio and technical impact.
**Medium**: Phase 1 tooling modernization.
**Ongoing**: Phase 4 & 5.

## Risks & Mitigations
- ABI stability: New features behind opt-in flags or new interfaces.
- Hardware driver maintenance: Focus effort on generic + DRM/KMS paths.
- Scope control: Every change must map back to an existing or new TAS block.

## Success Metrics
- Clean build + basic examples running on a modern embedded board.
- Measurable low-overhead rendering path for AI result visualization.
- Clear, citable narrative of unique maintainer position.

## Next Steps

This roadmap was generated as part of ongoing meta-infrastructure work. Contributions, discussions, and prioritization feedback are welcome via issues or pull requests.

**Maintained by**: Denis Oliver Kropp
**License**: LGPL-2.0-or-later

---

*This document lives in the repository to serve as living guidance for the evolution of DirectFB.*