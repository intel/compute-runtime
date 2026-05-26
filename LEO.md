<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# LEO -- Level Zero Executing OpenCL

## Overview

LEO is an experimental project within the Intel(R) Graphics Compute Runtime that re-implements the entire OpenCL(TM) API from scratch on top of [Level Zero (L0)](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/index.html). The design exports all OpenCL entry points directly from the `ze_intel_gpu` library and ultimately registers it as an OpenCL ICD (Installable Client Driver) implementation. Currently `igdrcl` will forward api entries to `ze_intel_gpu` if LEO enabled.

Rather than maintaining two parallel driver stacks, LEO maps every OpenCL API call onto Level Zero primitives -- allowing applications that target OpenCL to run through the same command-submission and memory-management infrastructure used by native Level Zero workloads.

## Motivation

| Goal | Description |
|------|-------------|
| **Single driver stack** | Eliminate duplication between the OpenCL and Level Zero backends by consolidating all GPU interaction into the L0 path. |
| **Reduced maintenance** | A single command-submission layer means fewer code paths to maintain, test, and enable for new hardware platforms. |
| **Increased test coverage** | Both APIs exercise identical driver paths, so every Level Zero test implicitly validates OpenCL behavior and vice versa -- significantly expanding overall test coverage without duplicating test infrastructure. |
| **Performance gains from L0 optimizations** | OpenCL workloads automatically benefit from the latest performance optimizations implemented in the Level Zero backend, without requiring separate tuning effort in the OpenCL stack. |

## Architecture

```
 +----------------------------------+
 |        Application               |
 |   (OpenCL API calls)             |
 +----------+-----------------------+
            |
 +----------v-----------------------+
 |   LEO - OpenCL entry points      |
 |   exported from ze_intel_gpu     |
 |   (registered as OpenCL ICD)     |
 +----------+-----------------------+
            |  Maps cl_* -> ze_*
 +----------v-----------------------+
 |   Level Zero Backend             |
 |   zeCommandListAppendLaunchKernel|
 |   zeCommandQueueExecute...       |
 +----------+-----------------------+
            |
 +----------v-----------------------+
 |   Kernel-Mode Driver (KMD)       |
 |   Intel GPU hardware             |
 +----------------------------------+
```

OpenCL objects such as `cl_command_queue`, `cl_mem`, and `cl_kernel` are internally backed by their Level Zero equivalents. All GPU work submission flows through the L0 command-queue infrastructure. The current implementation maps OpenCL objects to Level Zero objects in the following way:

| OpenCL Object | Level Zero Equivalent |
|---|---|
| Buffer | USM pointer |
| Command queue | Immediate command list with copy offload |
| Context | Context |
| Device | Device |
| Image | Image |
| Platform | Driver |
| Sampler | Sampler |
| Event | Counter-based event |
| User event | Regular event |

## Current Status

> **Experimental** -- LEO is under active development and is not yet intended for production use.

- Core compute paths (kernel dispatch, memory operations) are functional.
- Not all OpenCL extensions and edge cases are fully mapped.
- Performance characterization is ongoing.