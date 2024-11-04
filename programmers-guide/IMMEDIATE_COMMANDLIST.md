<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Level Zero Immediate Command Lists

* [Introduction](#Introduction)
* [Availability](#Availability)
* [Optimizations](#Optimizations)
* [Debug Keys](#Debug-Keys)
* [Notes](#Notes)
* [References](#References)

# Introduction

Immediate command lists is a feature provided by Level-Zero specification to allow for very low latency submission usage models. In this scheme, commands appended on the command list such as launching a kernel or performing a memory copy are immediately submitted to the device for execution. This is different from a regular command list where multiple commands can be stitched and submitted together for execution.

Distinctions between an immediate command list compared to a regular command list include (but not limited to) the following:

* An immediate command list is an implicit command queue and is therefore created using a command queue descriptor.
* Commands appended to an immediate command list are submitted for execution immediately on the device.
* Immediate command lists are not required to be closed or reset.
* Synchronization of immediate command lists cannot be performed by user via zeCommandQueueSynchronize as the user will not have a command queue handle or via a fence handle as fences are associated with a command queue. Proper synchronization models are detailed further below.

Since the intention of immediate command lists are to primarily provide a razor thin submission interface to the device, they are well suited to be used in workloads that have tendency to launch small or short running kernels and also need to run multiple iterations of such kernels. Examples of workloads with such characteristics can be found in HPC environments and also ML/DL frameworks.

## Programming model

Pseudo-code for creating immediate command list (async mode):
```cpp
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = queueGroupOrdinal;
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList);
```

Submitting commands and synchronization:
Launching kernels:
```cpp
    zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                    events[0], 0, nullptr);
    // If Async mode, use event for sync
    zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max() - 1);
```

Performing copies:
```cpp
    zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, hostBuffer, allocSize,
                                    events[0],
                                    0, nullptr);

    zeCommandListAppendMemoryCopy(cmdList, stackBuffer, deviceBuffer, allocSize,
                                    events[1],
                                    1,
                                    &events[0]);

    zeEventHostSynchronize(events[1], std::numeric_limits<uint64_t>::max() - 1);
```

Pseudo-code for creating immediate command list (sync mode):
```cpp
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = queueGroupOrdinal;
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList);
```

Launching kernel:
```cpp
    zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                    nullptr, 0, nullptr);
```

For sync mode immediate command lists, synchronization is performed implicitly, so it is not required to use events to synchronize.

For more code samples, please refer [compute-benchmarks](https://github.com/intel/compute-benchmarks/) repo. Scenarios such as  `create_command_list_immediate_l0.cpp` and `execute_command_list_immediate_l0.cpp` serve as good starting points.

# Availability

* Level-Zero support for immediate command list is available for all platforms except Meteor Lake.
* Immediate command lists support both Compute and Copy engines.

# Optimizations

* Optimized support using CSR flush task interface for immediate command lists is available by default on all platforms (Gen 9 onwards)
* Optimized CSR heap sharing functionality for immediate command list is available from Xe_HPG/Xe_HPC onwards.

# Debug Keys

* `EnableFlushTaskSubmission=0/1` : Force enable/disable support for using optimizations in non-pipelined state filtering for immediate command lists. Defaults to 1.
* `EnableImmediateCmdListHeapSharing=0/1` : Force enable/disable support for using CSR heap resources for immediate command list. Defaults to 1 from Xe_HPG/Xe_HPC onwards. When enabled, all immediate command lists created against same ordinal share GPU heaps instead of allocating separate heaps for each command list.

# Notes

* When using L0 Metrics in Query mode to capture execution metrics of an immediate command list, the time elapsed between zetCommandListAppendMetricQueryBegin and zetCommandListAppendMetricQueryEnd points may include time elapsed in the host as well.

# References

* https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#low-latency-immediate-command-lists
