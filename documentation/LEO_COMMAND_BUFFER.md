<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# cl_khr_command_buffer in LEO

`cl_khr_command_buffer` lets an application record a sequence of commands once and
replay it many times, which removes the per-enqueue host cost from a repeated
workload. See [LEO.md](LEO.md) for the driver this belongs to.

In LEO a command buffer is a regular (non-immediate) Level Zero command list.
Replay appends that list onto the immediate command list backing the target
OpenCL command queue, so a command buffer is a translation of an existing Level
Zero primitive rather than a new execution engine.

## Status: incomplete, off by default

> **Partially implemented** -- do not enable outside development.

Implemented: the command buffer object, creation, finalization, reference
counting, the info queries, and replay of an *empty* command buffer.

Not implemented: every command recording entry point
(`clCommandNDRangeKernelKHR`, `clCommandCopyBufferKHR`,
`clCommandFillBufferKHR`, ...), sync points, and
`cl_khr_command_buffer_mutable_dispatch`. A command buffer therefore cannot be
given any work yet.

## Enabling it

A consumer decides whether to use command buffers from what the driver
advertises. Advertising the extension before recording works would make such a
consumer enable command-graph support and then fail, instead of falling back to
plain enqueues. Everything that could reveal the extension is therefore gated
behind the `EnableClKhrCommandBuffer` debug variable, which defaults to disabled
and must stay disabled until the extension is complete and validated.

Being a debug variable does not prevent validating the extension on a release
build: set `NEOReadDebugKeys=1` (see [FAQ.md](FAQ.md)).

| Value | Meaning |
|-------|---------|
| `-1` | Default -- extension not advertised |
| `0` | Disabled |
| `1` | Enabled -- `cl_khr_command_buffer` is added to `CL_DEVICE_EXTENSIONS` and the `CL_DEVICE_COMMAND_BUFFER_*` queries answer |

With the default, the driver is indistinguishable from one without the feature:

| Route | Behaviour when disabled |
|-------|-------------------------|
| `CL_DEVICE_EXTENSIONS` | unchanged, no `cl_khr_command_buffer` entry |
| `CL_DEVICE_COMMAND_BUFFER_*` queries | `CL_INVALID_VALUE` |
| `clGetExtensionFunctionAddress` | `NULL` for every command-buffer entry point |
| `clCreateCommandBufferKHR` | `CL_INVALID_OPERATION` |

## What is reported when enabled

`CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR` reports no optional capability, and
both `CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR` and
`CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR` report no properties,
because nothing beyond an empty buffer has been validated.

The recorded command list mirrors the ordering and copy-offload behaviour of the
queue the buffer is created with, so a recorded command behaves like the
equivalent enqueue on that queue. Those are fixed at creation, so replaying onto
a different queue does not take on that queue's ordering.

A replay may target a queue other than the one the buffer was created with, as
long as it is on the same device and in the same context.

## Validation

The extension is validated with the OpenCL-CTS `test_command_buffer` suite and
with the Unified Runtime `exp_command_buffer` conformance suite driven through
the UR OpenCL adapter. UR is the consumer that matters: SYCL command-graph
support over the OpenCL backend is what this extension unlocks.
