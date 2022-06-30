<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Level Zero Immediate Commandlist

* [Introduction](#Introduction)
* [Availability](#Availability)
* [Debug Keys](#Debug-Keys)
* [Limitations](#Limitations)
* [References](#References)

# Introduction

Immediate command lists is a feature provided by Level-Zero specification to allow for very low latency submission usage models. In this scheme, commands appended on the command list such as launching a kernel or performing a memory copy are immediately submitted to the device for execution. This is different from a regular command list where multiple commands can be stitched and submitted together for execution .

Distinctions between an immediate command list compared to a regular command list include (but not limited to) the following:

* An immediate command list is an implicit command queue and is therefore created using a command queue descriptor.
* Commands appended to an immediate command list are submitted for execution immediately on the device.
* Immediate command lists are not required to be closed or reset.
* Synchronization of immediate command lists cannot be performed by user via zeCommandQueueSynchronize as the user will not have a command queue handle or via a fence handle as fences are associated with a command queue. Proper synchronization models are detailed further below.

Since the intention of immediate command lists are to primarily provide a razor thin submission interface to the device, they are well suited to be used in workloads who have tendency to launch small or short running kernels and also need to run multiple iterations of such kernels. Examples of workloads with such characteristics can be found in HPC environments and also ML/DL frameworks.

## Programming model

Pseudo-code for creating immediate command list (async mode):
```
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
```
    zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                    events[0], 0, nullptr);
    // If Async mode, use event for sync
    zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max() - 1);
```

Performing copies:
```
    zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, hostBuffer, allocSize,
                                    events[0],
                                    0, nullptr);

    zeCommandListAppendMemoryCopy(cmdList, stackBuffer, deviceBuffer, allocSize,
                                    events[1],
                                    1,
                                    &events[0]);

    zeEventHostSynchronize(events[1], std::numeric_limits<uint64_t>::max() - 1));
```

Pseudo-code for creating immediate command list (sync mode):
```
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
```
    zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                    nullptr, 0, nullptr);
```

For sync mode immediate command lists, synchronization is performed implicitly, so it is not required to use events to synchronize.

For more code samples, please refer [compute-benchmarks](https://github.com/intel/compute-benchmarks/) repo. Scenarios such as  `create_command_list_immediate_l0.cpp` and `execute_command_list_immediate_l0.cpp` serve as good starting points.

# Availability

* Level-Zero support for immediate command list is available for all platforms
* Optimized support for immediate command lists is currently available by default on DG2 and PVC only.
* Immediate command lists support both Compute and Copy engines.

# Debug Keys

* Users can force optimized immediate command list by using `EnableFlushTaskSubmission=1`.

# Limitations
 Usage of `EnableFlushTaskSubmission=1` has been verified to to work on XeHP_SDV, PVC and DG2 only. So, it may or may not work depending on platform being used. Support for optimized immediate command lists for more platforms is work in progress.
 
# References

* https://one-api.gitlab-pages.devtools.intel.com/level_zero/core/PROG.html#low-latency-immediate-command-lists
