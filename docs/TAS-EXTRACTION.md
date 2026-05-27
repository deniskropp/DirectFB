# TAS Extraction — DirectFB 1.x Architecture

**Repository**: deniskropp/DirectFB  
**Date**: 2026-05-27  
**Context**: Architectural decomposition into Task-Agnostic Steps for modernization and AI integration.

## Overview

DirectFB is decomposed into composable, atomic Task-Agnostic Steps (TAS) across 6 architectural layers. Each TAS captures a clear responsibility with defined inputs, outputs, and modernization hooks.

## Layer 1: Core & Factory

### TAS-DFB-CORE-001: IDirectFB Factory & Lifecycle
- **Description**: Creation, initialization, and shutdown of the main `IDirectFB` interface.
- **Inputs**: Configuration (`directfbrc`, CLI, environment)
- **Outputs**: `IDirectFB` handle
- **Key locations**: `include/directfb.h`, `src/core/`
- **Modernization hook**: Add C++ RAII / smart-pointer wrappers; expose clean factory for AI pipelines.

### TAS-DFB-CORE-002: Resource Registry & Reference Counting
- Manages surfaces, layers, input devices with proper lifetime tracking.

## Layer 2: Surfaces & Rendering

### TAS-DFB-SURF-001: Surface Management
- Creation, locking/unlocking, blitting, stretching, color keying.
- **Modernization hook**: Zero-copy or efficient import from AI inference tensors (TensorRT, ONNX Runtime, etc.).

### TAS-DFB-SURF-002: Hardware Acceleration Dispatch
- Routes rendering calls to the active graphics driver.

### TAS-DFB-SURF-003: Pixel Format Negotiation
- Format conversion and capability matching.

## Layer 3: Display & Layers

### TAS-DFB-LAYER-001: Display Layer & Mode Management
- Layer enumeration, mode setting via `fb.modes` or equivalent.
- **Key**: `src/display/`, framebuffer device handling.

### TAS-DFB-LAYER-002: Layer Composition
- Stacking, opacity, region management.

## Layer 4: System & Backend Abstraction

### TAS-DFB-SYS-001: System Backend Loading (fbdev primary)
- Primary Linux framebuffer (`/dev/fb0`) access.
- **Modernization hook (High Priority)**: Add/strengthen DRM/KMS backend (following DirectFB2 direction) while keeping fbdev for minimal-overhead legacy devices.

### TAS-DFB-SYS-002: Cross-Platform Abstraction
- Support for SDL (experimental), X11, and future backends.

## Layer 5: Drivers

### TAS-DFB-GFX-001: Graphics Driver Interface
- Hardware-specific acceleration (blit, fill, draw hooks).
- Located in `gfxdrivers/`.

### TAS-DFB-INP-001: Input Device Abstraction
- Keyboard, mouse, and other input event handling and dispatching.
- Located in `inputdrivers/`.

## Layer 6: Multi-Application & Windowing (Fusion)

### TAS-DFB-FUSION-001: Master/Slave Process Model & IPC
- Shared memory (`/dev/shm/fusion.*`), kernel module (`linux-fusion`), safe cross-process access to graphical resources.
- **Modernization hook**: Document current kernel compatibility; explore user-space alternatives or updates if needed.

### TAS-DFB-FUSION-002: Shared Resource Safety
- Thread/process-safe access to surfaces and layers.

### TAS-DFB-WM-001: Lightweight Window Manager
- Stacking, focus, resizing, opacity control (Meta/CapsLock shortcuts).
- Includes SaWMan and built-in WM.

## Cross-Cutting Concerns

- **TAS-DFB-IFACE-001**: Pluggable Interface Providers (image, font, video) via `interfaces/`
- **TAS-DFB-BUILD-001**: Dual autotools + CMake build system + cross-compilation
- **TAS-DFB-CONFIG-001**: Runtime configuration system
- **TAS-DFB-CODEGEN-001**: Flux IDL for interface/IPC code generation

## Measurement & Validation Hooks

- Surface lock/blit latency
- Memory footprint per surface/layer
- Multi-app context-switch overhead
- Driver acceleration coverage

These TAS blocks provide a clean foundation for incremental modernization while preserving DirectFB’s core strengths: minimal overhead and embedded focus.