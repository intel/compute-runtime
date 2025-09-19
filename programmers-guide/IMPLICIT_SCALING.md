<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Implicit Scaling

* [Overview](#Overview)
* [Availability](#Availability)
* [Debug Keys](#Debug-Keys)
* [Limitations](#Limitations)

# Overview

Multi-tile devices, such as Xe HPC (PVC), contain smaller GPU devices called tiles. Each tile has its own dedicated set of resources.

* Each tile has Execution Units (EUs) doing the actual computation work, which can be accessed using the available devices.
* Similarly, copy engines (BCSs) may be present on each tile.
* Each tile has HBM (High-Bandwidth Memory) local memory that is allocated directly on the chip.
* Each tile has its own portion of L3 cache to speed up things.

For a system like this, the following UMD (User Mode Driver) device handles are exposed:

* Root device handle.
* Sub-device handle for each tile.

To manage the resources on those sub-devices, the UMD introduces two main development models:

* *Implicit scaling* model, on which application allocates and submits to the root device and driver is responsible for distribution of work and memory across tiles.
* *Explicit scaling* model, on which application is responsible for distributing work and memory across tiles using sub-device handles.

When doing allocations in implicit scaling mode, driver *colors* an allocation among the available tiles. Default coloring divides an allocation size evenly by the number of available tiles. Other policies include dividing the allocation in chunks of a given size, which are then interleaved on each tile.

When scheduling a kernel for execution, driver distributes the kernel workgroups among the available tiles. Default mechanism is called *Static Partitioning*, where the workgroups are evenly distributed among tiles. For instance, in a 2-tile system, half of the workgroups go to tile 0, and the other half to tile 1.

The number of CCSs, or compute engines, currently available with implicit scaling on the root device is one. This is because with implicit scaling the driver automatically uses all the EUs available in the device, so no other CCSs are exposed. Even though only one CCS is exposed, multiple kernels submitted to the root device using implicit scaling may execute concurrently on PVC, depending on EU availability.

No implicit scaling support is available for BCSs. Considering that, two models are followed in terms of discovery of copy engines:

* In Level Zero, the copy engines from sub-device 0 are exposed also in the root device. This is to align the engine model on both the implicit and the non-implicit-scaling scenarios.
* In OpenCL, copy engines are not exposed in the root device.

Since implicit scaling is only done for EUs, which are associated only with kernels submitted to CCS, BCSs are currently not being exposed and access to them are done through sub-device handles.

# Availability

* OpenCL has implicit scaling enabled by default on all platforms.
* Level Zero has it enabled by default on Xe HPC (PVC) B and later steppings.

# Debug Keys

## Implicit scaling enabling

Implicit scaling can be enabled and disabled in Level Zero by setting the `EnableImplicitScaling` debug key to `1` or `0`, respectively.

When implicit scaling is enabled, the root device is seen as a monolithic device, with internal resources being managed by the driver. When it is disabled, the root device has the same resources as tile 0, and kernel submissions and allocations to both the root device handle and tile 0 device handle have the same effect.

In OpenCL, implicit scaling is always enabled. To not use implicit scaling with OpenCL, the affinity mask targeting one of the sub-devices may be used, e.g., `ZE_AFFINITY_MASK=0.0`.

## Coloring scheme

By default, allocations are equally split among available tiles. Allocations can also be colored by chunks and interleaved on each tile (`MultiStoragePolicy=1` or `MultiStoragePolicy=2`, respectively) with a size set on `MultiStorageGranularity`, which must be equal or greater than 64 kB, with 64 kB being the default.
