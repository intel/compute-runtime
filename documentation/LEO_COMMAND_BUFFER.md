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
counting, the info queries, replay, sync points, and recording with
`clCommandNDRangeKernelKHR`, `clCommandCopyBufferKHR`,
`clCommandCopyBufferRectKHR`, `clCommandFillBufferKHR`,
`clCommandBarrierWithWaitListKHR`, `clCommandSVMMemcpyKHR` and
`clCommandSVMMemFillKHR`.

Not implemented: the image commands, kernels using printf or with image or
shared object arguments, and `cl_khr_command_buffer_mutable_dispatch`.

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

`CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR` reports no optional capability.
`CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR` reports
`CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` and `CL_QUEUE_PROFILING_ENABLE`, and
`CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR` reports none.

Commands are recorded into a Level Zero command list with the ordering of the
queue the command buffer is created with. On an in-order list, recording order
satisfies every sync point. On an out-of-order list, commands may run
concurrently, and a command with a non-empty sync point wait list is recorded
after a barrier. A sync point can only name an earlier command, so the barrier
satisfies it.

A command buffer may be enqueued again while an earlier enqueue of it is still
executing, as long as the new enqueue depends on the earlier one, for example
through an in-order queue, an event or a barrier. An in-order recorded list
uses the patch preamble, so each execution initializes its in-order counter on
the GPU, in submission order.

A command buffer holds a reference to every kernel and memory object it
records, so it stays valid after the application releases them. As the
extension specifies, objects set as kernel arguments are not retained: the
application keeps them until the command buffer is released.

A command buffer may be released while it is still executing. The release
waits for every queue the command buffer was enqueued on before the recorded
list is destroyed.

A replay may target a queue other than the one the buffer was created with, as
long as it is in the same context, on the same device and on the same engine
group. Its ordering and profiling may differ: the recorded list keeps the
ordering of the creating queue. A queue or a wait event from another context
returns `CL_INVALID_CONTEXT`, a queue on another device returns
`CL_INVALID_DEVICE`, and a queue on another engine group returns
`CL_INCOMPATIBLE_COMMAND_QUEUE_KHR`.

The recorded commands carry no performance counter query. A replay on a queue
with performance counters (MDAPI) enabled returns
`CL_INCOMPATIBLE_COMMAND_QUEUE_KHR`, also when that queue is the creating queue.

## Validation

The extension is validated with the OpenCL-CTS `test_command_buffer` suite and
with the Unified Runtime `exp_command_buffer` conformance suite driven through
the UR OpenCL adapter. UR is the consumer that matters: SYCL command-graph
support over the OpenCL backend is what this extension unlocks.
