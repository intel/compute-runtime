<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# NEO Architecture Overview

This document describes the high-level architecture of the Intel(R) Graphics
Compute Runtime (NEO): how the source tree is organized, how the major
components relate to each other, and how a request travels from an application
API call down to a hardware submission. It is intended as an entry point for
readers who want to understand the project as a whole before diving into a
specific subsystem.

For build instructions see [BUILD.md](BUILD.md). For coding and contribution
rules see [GUIDELINES.md](GUIDELINES.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
For feature-specific deep dives see the
[Programmers Guide](programmers-guide/PROGRAMMERS_GUIDE.md).

* [Layered structure](#layered-structure)
* [Top-level source layout](#top-level-source-layout)
* [The shared core](#the-shared-core)
* [Object and ownership model](#object-and-ownership-model)
* [Hardware abstraction](#hardware-abstraction)
* [Operating-system abstraction](#operating-system-abstraction)
* [Memory management](#memory-management)
* [Command submission path](#command-submission-path)
* [Compiler interface](#compiler-interface)
* [End-to-end runtime flows](#end-to-end-runtime-flows)
* [Testing layout](#testing-layout)

## Layered structure

NEO is a single codebase that exposes two compute APIs on top of a common core.
There are three top-level layers:

```
   +-----------------------------+   +-----------------------------+
   |        opencl/              |   |        level_zero/          |
   |   OpenCL(TM) frontend       |   |   oneAPI Level Zero frontend|
   |   (cl_* entry points,       |   |   (ze*/zes* entry points,   |
   |    CommandQueue, MemObj,    |   |    CommandList, Module,     |
   |    Kernel, Program, ...)    |   |    Kernel, Event, ...)      |
   +--------------+--------------+   +--------------+--------------+
                  |                                 |
                  +----------------+----------------+
                                   |
                  +----------------v----------------+
                  |             shared/             |
                  |   Common runtime core:          |
                  |   Device, CommandStreamReceiver,|
                  |   MemoryManager, command         |
                  |   container/encoder, compiler    |
                  |   interface, helpers, HW + OS    |
                  |   abstraction                    |
                  +----------------+----------------+
                                   |
                  +----------------v----------------+
                  |  OS kernel-mode driver + HW     |
                  |  (Linux i915/xe DRM, Windows     |
                  |   WDDM)                          |
                  +---------------------------------+
```

* `opencl/` and `level_zero/` are **API frontends**. They translate API objects
  and calls into operations on shared-core objects. They do not talk to the
  hardware or OS directly.
* `shared/` is the **runtime core**. It owns device enumeration, memory
  management, command generation, submission, synchronization, and all
  hardware- and OS-specific behavior.

A key consequence: most non-trivial functional behavior lives in `shared/`, and
the two frontends are comparatively thin. When changing behavior, prefer
implementing it once in `shared/` and exposing it through both frontends rather
than duplicating it.

## Top-level source layout

| Path | Responsibility |
|------|----------------|
| `shared/source/` | The runtime core (see [The shared core](#the-shared-core)). |
| `shared/test/` | Unit tests (ULTs) for shared code. |
| `shared/offline_compiler/` | `ocloc` offline kernel compiler tool. |
| `opencl/source/` | OpenCL API implementation. |
| `opencl/test/` | OpenCL ULTs. |
| `level_zero/core/` | Level Zero core API (devices, command lists/queues, modules, kernels, events). |
| `level_zero/sysman/` | Level Zero System Management (power, frequency, temperature, RAS, ...). |
| `level_zero/tools/` | Level Zero tools API (metrics, debug, sysman tools). |
| `level_zero/experimental/` | Experimental Level Zero extensions. |
| `programmers-guide/` | Feature-oriented design and optimization guides. |
| `third_party/` | Vendored external headers and sources. |
| `manifests/` | Component revision pins used by the build / CI. |

Within each API frontend the object directories follow the API's own
vocabulary: OpenCL has `platform`, `cl_device`, `context`, `command_queue`,
`mem_obj`, `program`, `kernel`, `event`, `sampler`; Level Zero has `driver`,
`device`, `context`, `cmdqueue`, `cmdlist`, `module`, `kernel`, `event`,
`fence`, `image`, `sampler`.

## The shared core

The most important directories under `shared/source/`:

| Directory | Responsibility |
|-----------|----------------|
| `execution_environment/` | Process-wide root object and per-root-device environment. |
| `device/` | `Device`, `RootDevice`, `SubDevice` and device capabilities. |
| `command_stream/` | `CommandStreamReceiver` (CSR) - command buffer assembly, submission, synchronization. |
| `command_container/` | `CommandContainer` and the `Encode*` command encoders that emit hardware commands. |
| `commands/` | Hardware command and structure definitions. |
| `memory_manager/` | `MemoryManager`, `GraphicsAllocation`, GPU address-space partitioning, host-pointer tracking. |
| `direct_submission/` | Ring-buffer submission path that bypasses per-submission kernel-mode calls. |
| `os_interface/` | OS abstraction (`OSInterface`, device discovery, contexts) with Linux and Windows backends. |
| `compiler_interface/` | Interface to the Intel Graphics Compiler (IGC) and kernel build flow. |
| `built_ins/` | Built-in kernels (copy, fill, ...) and the System Routine (SIP) for debugging. |
| `device_binary_format/` | Parsing/packing of device binaries (zebin, patchtokens). |
| `program/` | Program/kernel binary handling shared by both APIs. |
| `helpers/` | Hardware- and product-aware helper classes (see below). |
| `release_helper/` | Per-IP-release feature/workaround switches. |
| `gmm_helper/` | Integration with GmmLib (tiling, cache settings, page tables). |
| `debug_settings/` | Debug keys / environment-variable controlled behavior. |
| `debugger/` | GPU debugger (tools) support. |
| `gen12lp/`, `xe_hpg_core/`, `xe_hpc_core/`, `xe2_hpg_core/`, `xe3_core/`, `xe3p_core/` | Per-graphics-family hardware code (see [Hardware abstraction](#hardware-abstraction)). |

## Object and ownership model

NEO builds a tree of long-lived objects during initialization. Ownership flows
top-down; child objects hold non-owning back-pointers to their parents.

```
ExecutionEnvironment                 (one per process)
  |- MemoryManager                   (one, shared by all root devices)
  |- DirectSubmissionController
  |- OsEnvironment
  |- rootDeviceEnvironments[]        (one RootDeviceEnvironment per root device)
       |- HardwareInfo               (platform, feature table, capabilities)
       |- OSInterface                (DRM or WDDM backend)
       |- GfxCoreHelper / ProductHelper / ReleaseHelper / CompilerProductHelper
       |- GmmHelper, CompilerInterface, BuiltIns, Debugger, ...
       |- MemoryOperationsHandler

Device  (RootDevice)                 (created per root device, references its RootDeviceEnvironment)
  |- SubDevice[]                      (one per tile on multi-tile parts)
  |- allEngines : EngineControl[]    (one entry per engine instance the device uses)
        |- CommandStreamReceiver     (owns command buffers, tag/fence, residency)
        |- OsContext                 (the OS-level submission context for an engine)
```

* **`ExecutionEnvironment`** is the process-wide root. It owns the single
  `MemoryManager` and the vector of `RootDeviceEnvironment` objects. It is
  reference-counted so it outlives every device that references it.
* **`RootDeviceEnvironment`** holds everything that is per-physical-device but
  not per-API-object: the `HardwareInfo`, the `OSInterface`, and the whole set
  of hardware/product helper objects.
* **`Device`** is the runtime's notion of a usable device. A `RootDevice` may
  expose `SubDevice` children (one per tile). A `Device` owns its engines.
* **`EngineControl`** pairs a `CommandStreamReceiver` with an `OsContext`. Each
  engine instance (e.g. a compute engine, a copy engine) that the device uses
  has its own `EngineControl`. `OsContext` carries the engine type and usage
  (regular, low/high priority, internal, cooperative).

The API frontends wrap these: an OpenCL `ClDevice` or a Level Zero `Device`
(L0) holds a pointer to the shared `Device`; a `CommandQueue`/`CommandList`
ultimately routes work to an `EngineControl`'s `CommandStreamReceiver`.

## Hardware abstraction

NEO supports many graphics families from a single codebase using **template
specialization on a `GfxFamily` type** plus a file-naming convention. This is
the most distinctive structural pattern in the tree and is described in detail
in [Hardware Abstraction](programmers-guide/HARDWARE_ABSTRACTION.md). In short:

* Family-independent algorithms are written once as templates
  (`class CommandStreamReceiverHw<GfxFamily>`, `struct EncodeDispatchKernel<GfxFamily>`,
  ...). The `GfxFamily` type supplies the concrete hardware command structs
  (e.g. `GfxFamily::PIPE_CONTROL`).
* Implementation bodies live in `.inl` files whose suffix encodes the range of
  families they apply to (e.g. `_base.inl`, `_xehp_and_later.inl`,
  `_from_xe_hpc_to_xe3.inl`). Per-family `.cpp` files in the family directories
  instantiate the templates for a concrete family.
* Behavioral differences that are not worth a full template specialization are
  expressed through **helper objects** selected at runtime:
  * `GfxCoreHelper` - architecture-level (per graphics family) behavior.
  * `ProductHelper` - product/SKU-level behavior and workarounds.
  * `ReleaseHelper` - per-IP-release feature and workaround switches.
  * `CompilerProductHelper` - product-specific compiler-facing details.
  * `AILConfiguration` (Application Intelligence Layer) - per-application
    behavior tweaks based on the running process.

## Operating-system abstraction

OS-specific behavior is isolated behind `OSInterface` and a set of backends.
Two operating systems are supported from the same sources: Linux (using the
i915 or xe DRM kernel-mode driver) and Windows (using WDDM). The Linux path is
the one that is buildable from this repository; the Windows path shares the
codebase but is built elsewhere.

The convention used throughout `shared/source/os_interface/` (and mirrored in
other directories) selects the backend by file-name suffix:

* `*_drm.cpp` - Linux/DRM implementation.
* `*_wddm.cpp` - Windows/WDDM implementation.
* `*_drm_or_wddm.cpp` - selects between the two at build time when a single
  binary may target either.
* `linux/` and `windows/` subdirectories hold larger backend-specific bodies.

The same suffix convention appears in `direct_submission/`, `os_interface/`, and
elsewhere. The OS layer is responsible for device discovery, allocating and
mapping memory through the kernel-mode driver, creating submission contexts
(`OsContext`), and submitting command buffers.

## Memory management

A single `MemoryManager` (owned by `ExecutionEnvironment`) serves all root
devices. Allocations are represented by `GraphicsAllocation` objects; a
`MultiGraphicsAllocation` groups the per-root-device copies of one logical
allocation.

Key concepts:

* **Allocation type** (`allocation_type.h`) - the purpose of an allocation
  (buffer, image, kernel ISA, internal heap, command buffer, ...). It drives
  placement, caching, and residency decisions.
* **`GfxPartition`** - partitions the GPU virtual address space into heaps
  (e.g. standard, standard64K, SVM, heap for allocations addressable in 32 bits).
  See [Allocations greater than 4GB](programmers-guide/ALLOCATIONS_GREATER_THAN_4GB.md).
* **Host-pointer management** (`HostPtrManager`) - tracks user-provided host
  memory that must be made resident for the GPU.
* **Residency** - before a submission, every `GraphicsAllocation` the work
  touches must be made resident in the engine's context. The CSR collects these
  into a residency container and the OS layer commits them.
* **Deferred deletion** - allocations still referenced by in-flight work are
  retired asynchronously rather than freed immediately.

For Unified Shared Memory placement details see
[System Memory Allocations](programmers-guide/SYSTEM_MEMORY_ALLOCATIONS.md).

## Command submission path

Work reaches the hardware through the `CommandStreamReceiver` (CSR). The CSR is
the heart of submission and synchronization:

1. The frontend records work into a command buffer (a `LinearStream` backed by a
   `GraphicsAllocation`), using the `Encode*` encoders in `command_container/`
   to emit family-correct hardware commands.
2. State that must precede the work (state base address, pipeline select,
   preemption, scratch space, ...) is programmed by the CSR via `flushTask` /
   `flushImmediateTask`.
3. Allocations touched by the work are added to the **residency** set.
4. The batch buffer is **flushed**: either through the classic per-submission
   path (a kernel-mode submit call) or through **direct submission**, where the
   driver keeps a ring buffer resident and rings a doorbell, avoiding a
   kernel-mode call per submit (see `shared/source/direct_submission/`).
5. Completion is tracked through a monotonically increasing **task count**
   written by the GPU to a **tag allocation**; host waits poll or block on this
   tag (with an optional kernel-mode notify fallback).

`CommandContainer` owns the command buffer and the indirect heaps (surface
state, dynamic state) for a sequence of commands. The `Encode*` structs (e.g.
`EncodeDispatchKernel`, `EncodeStateBaseAddress`, `EncodeSemaphore`) are the
single place where hardware command bits are written, which keeps command
generation consistent across both APIs.

For the immediate-command-list submission model see
[Immediate Commandlist](programmers-guide/IMMEDIATE_COMMANDLIST.md).

## Compiler interface

Kernels are compiled by the Intel Graphics Compiler (IGC), reached through
`compiler_interface/`. The runtime passes source (OpenCL C, or SPIR-V for Level
Zero) and build options to IGC and receives a **device binary**.
`device_binary_format/` parses the result (zebin or legacy patchtokens) into a
`KernelDescriptor` describing arguments, binding-table layout, required SLM,
and other dispatch metadata. The runtime also ships **built-in kernels**
(`built_ins/`) for operations like copy and fill, and a **System Routine
(SIP)** used during debugging. Compiled binaries can be cached on disk; see
[NEO Compiler Cache](programmers-guide/COMPILER_CACHE.md).

## End-to-end runtime flows

**Initialization / device discovery**

1. `ExecutionEnvironment` is created and `prepareRootDeviceEnvironments` is
   called.
2. The OS layer discovers devices (`device_factory.cpp` /
   `discover_devices_*`), creating one `RootDeviceEnvironment` per device and
   populating `HardwareInfo` and `OSInterface`.
3. Helper objects (`GfxCoreHelper`, `ProductHelper`, `ReleaseHelper`,
   `CompilerProductHelper`) are constructed for each device's family/product.
4. The shared `MemoryManager` is initialized.
5. `Device`/`RootDevice` objects are created, sub-devices and engines are set
   up, and each engine gets a `CommandStreamReceiver` + `OsContext`.
6. The API frontend wraps the shared devices and exposes them through the API.

**Memory allocation**

1. The frontend requests an allocation from the `MemoryManager` with
   `AllocationProperties` (size, type, flags).
2. The manager chooses a heap via `GfxPartition`, applies product/GMM cache and
   tiling settings, and asks the OS layer to back and map the memory.
3. A `GraphicsAllocation` (or per-device set in a `MultiGraphicsAllocation`) is
   returned and later made resident at submission time.

**Kernel dispatch and submission**

1. The frontend resolves the kernel (already compiled into a `KernelDescriptor`)
   and the dispatch parameters (global/local sizes).
2. Commands are encoded into the command buffer via `EncodeDispatchKernel` and
   related encoders; required heaps and state are programmed.
3. Touched allocations are added to the residency set.
4. The CSR flushes the batch buffer (classic or direct submission).
5. The host synchronizes on the resulting task count when it needs the results.

Hot dispatch paths (`zeCommandListAppendLaunchKernel`,
`clEnqueueNDRangeKernel`) are performance-critical; new non-default behavior
should be gated behind early-outs so these paths stay fast (see
[GUIDELINES.md](GUIDELINES.md)).

## Testing layout

Tests are unit-level tests (ULTs) co-located with the code they validate:

* `shared/test/unit_test` - shared core.
* `level_zero/core/test/unit_tests` - Level Zero.
* `opencl/test/unit_test` - OpenCL.

Multi-threaded code is tested in the dedicated `mt_tests` suites. AUB tests
validate the exact command stream against a simulator. See
[GUIDELINES.md](GUIDELINES.md) for testing conventions.
