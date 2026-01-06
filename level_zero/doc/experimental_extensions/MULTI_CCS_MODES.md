<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Multi-CCS Modes

* [Overview](#Overview)
* [Functional and Performance Considerations](#Functional-and-Performance-Considerations)
* [Interaction with Affinity Mask](#Interaction-with-Affinity-Mask)
* [Availability](#Availability)

# Overview

Xe HPC (PVC) contains 4 CCSs (Compute-Command Streamers) per tile, which can be used to access a common pool of Execution Units (EUs). Hardware allows for the selection of a specific distribution of EUs among CCSs, such as:

- All EUs may be assigned to a single CCS, on which case, only 1 CCS is needed to be exposed to users.
- EUs may be distributed equally among the 4 CCSs, and all CCSs are exposed.

Applications query for the number of CCSs exposed in the target device by using Level Zero queue groups [https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#command-queue-groups](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#command-queue-groups).

Depending on their execution patterns, applications may benefit more from using one or other configuration: Some may benefit from using 1 single CCS to access all EUs, while others may benefit from using more than 1 CCS, each with a fixed number of assigned EUs. For instance:

- A single process job may benefit from using 1 CCS with access to all EUs.
- A two-process job per tile with uniform work may benefit more from using 2 CCSs, with half of EUs assigned to each.
- A two-process job per tile with non-uniform work may benefit from using 1 CCS with access to all EUs.
- A four-process job per tile with uniform work may benefit more from using 4 CCSs, with a quarter of EUs assigned to each.
- A four-process job per tile with non uniform work may benefit from using 1 CCS with access to all EUs.

To help applications select the best mode that fits their needs, Level Zero driver is introducing a new driver experimental environment variable, named `ZEX_NUMBER_OF_CCS`. This environment variable behaves differently for PVC.

For PVC, This environment variable is read at `zeInit()` time, after reading `ZE_AFFINITY_MASK`, and allows users to select one of the supported modes.

For non-PVC, This environment variable is read before loading KMD at `zeinit()` time, and allows users to select one of the supported modes.

Following are supported modes:

- 1 CCS Mode (DEFAULT): Each tile exposes 1 CCS, which has access to all EUS. Other CCSs are disabled.
- 2 CCS Mode: Each tile exposes 2 CCSs, each with half of the EUs assigned to it. If no work is submitted to one of the CCSs, then its EUs remain idle, even if the other CCS has active work.
- 4 CCS Mode: Each tile exposes 4 CCSs, each having a quarter of the EUs assigned to it. As with 2 CCS mode, EUs of idle CCSs cannot be used by other CCSs.

The format for `ZEX_NUMBER_OF_CCS` for PVC is a comma-separated list of device-mode pairs, i.e., `ZEX_NUMBER_OF_CCS=<Root Device Index>:<CCS Mode>,<Root Device Index>:<CCS Mode>...`. For instance, in dual-PVC system, an application could have the following to set root device index 0 in 4 CCS mode, and root device index 1 in 1 CCS mode.

`ZEX_NUMBER_OF_CCS=0:4,1:1`

The format for `ZEX_NUMBER_OF_CCS` for devices other than PVC is a single integer representing CCS mode, i.e `ZEX_NUMBER_OF_CCS=<CCS Mode>`.
CCS mode will be configured for all the supported devices. For instance, in multi card system, an application could have the following to set CCS mode to 2 for all cards.

`ZEX_NUMBER_OF_CCS=2`

For non-PVC, Level zero driver uses sysfs interface to configure CCS mode, i.e `/sys/class/drm/cardX/gt/gtY/ccs_mode`. System administrator must properly configure access to the mentioned sysfs file for configuring CCS mode. Recommendation is that access permissions are to be provided for video users group. Commands to do the same are described below.

`chown :video /sys/class/drm/cardX/gt/gtY/ccs_mode`
`chmod g+rw /sys/class/drm/cardX/gt/gtY/ccs_mode`

where X refers to GPU ID and Y refers to tile ID.

# Functional and Performance Considerations

- *What happens when multiple applications run concurrently with different modes?*
When an application submits work in a different mode than the currently being used by another application, submissions from the second application are blocked until all current submissions from the first application finish, since change in mode can only be made when GPU is idle. Mixing applications with different modes therefore should be highly avoided to prevent performance regressions.

- *What happens when submitting to only 1 CCS from multiple workloads?*
Since virtual engines are disabled in 1 CCS mode, all submissions go to the same engine, and these are serialized by GuC. GuC will time slice those submission, each process will get time quanta (default 5ms), after which GuC will try to preempt and switch to other workload.

- *What happens in a multi-process application, with all processes having similar workload?*
2 CCS or 4 CCS modes may be used, as they will ensure concurrent execution of all processes.

- *What happens if an application wants to use multiple CCS with unbalanced work?*
Better to use 1 CCS mode to ensure that all EUs are used by all queues and to avoid having the bigger workload using only a percentage of the EUs. GPU partitioning is static, so if you have small amount of work on one CCS, it will underuse 25/50% of statically assigned resources.

- *What happens with implicit scaling?*
When implicit is enabled, only part of each tile is used for the split workgroups. That is, 100% of each time with 1 CCS mode, 50% with 2 CCS mode, and 25% with 4 CCS mode. It is recommended then to use only 1 CCS mode with implicit scaling to avoid performance regressions with implicit scaling.

# Interaction with Affinity Mask

Following is applicable only for PVC.

`ZE_AFFINITY_MASK` is read by the Level Zero driver prior to `ZEX_NUMBER_OF_CCS`. Therefore, mask can hide some root devices and change its indexes and `ZEX_NUMBER_OF_CCS` would apply for root device indexes after masking. For instance, in a 4-PVC system, we could have:

- Process 0 - `ZE_AFFINITY_MASK=0.0`  `ZEX_NUMBER_OF_CCS=0:1`
- Process 1 - `ZE_AFFINITY_MASK=0.1`  `ZEX_NUMBER_OF_CCS=0:1`
- Process 2 - `ZE_AFFINITY_MASK=1.0`  `ZEX_NUMBER_OF_CCS=0:4`
- Process 3 - `ZE_AFFINITY_MASK=1.1`  `ZEX_NUMBER_OF_CCS=0:4`

Alternatively, a process may select different modes for each tile. For instance, the following line selects card 0's tile 1 with 4 CCSs, and card 1's tile 0 with 2 CCSs:

- `ZE_AFFINITY_MASK=0.1,1.0`  `ZEX_NUMBER_OF_CCS=0:4,1:2`

# Availability

- `ZEX_NUMBER_OF_CCS` can be used also by applications using Intel OpenCL GPU driver.