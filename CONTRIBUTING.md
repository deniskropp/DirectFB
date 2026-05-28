# Contributing to DirectFB 1.x

Thank you for your interest in contributing to DirectFB!

This repository maintains the original **DirectFB 1.x** architecture, originally created by Denis Oliver Kropp and the DirectFB community.

## How to Contribute

1. **Check the Roadmap First**
   - Review [MODERNIZATION-ROADMAP.md](MODERNIZATION-ROADMAP.md) for current priorities (especially Phase 0–3).
   - Focus areas: DRM/KMS backend, AI surface integration, build modernization, documentation revival.

2. **Issues & Discussions**
   - Open an issue to discuss bugs, feature requests, or modernization ideas.
   - For large changes, please reference the relevant TAS (Task-Agnostic Step) from the architectural analysis.

3. **Pull Requests**
   - Fork the repository and create a feature branch.
   - Keep changes focused and well-documented.
   - Update or add documentation where appropriate.
   - Ensure the code builds (`./autogen.sh && ./configure && make`).

4. **Code Style**
   - Follow the existing [CODING_STYLE](CODING_STYLE) guidelines.
   - Maintain compatibility with the original architecture and ABI where possible.

5. **Testing**
   - Test on embedded Linux targets when possible.
   - For multi-application features, verify with the `linux-fusion` module.

## Communication

- GitHub Issues and Pull Requests are the primary channels.
- For maintainer coordination, reference ongoing meta-infrastructure work or open discussions in the PR.

## License

All contributions are licensed under the same terms as the project: **LGPL-2.0-or-later**.

---

*DirectFB's strength has always been its minimalism and performance on constrained hardware. We aim to preserve that while evolving it for new use cases.*