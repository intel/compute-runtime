<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# NUMA Aware Host USM Placement

* [Introduction](#introduction)
* [Requirements](#requirements)
* [Node selection](#node-selection)
* [Device selection](#device-selection)
* [Binding the allocating thread](#binding-the-allocating-thread)
* [Debug keys](#debug-keys)

# Introduction

A host USM allocation (`zeMemAllocHost`, `clHostMemAllocINTEL`) is created
through one device of the context. On devices that report memory regions to the
driver, the kernel driver allocates the pages of a host allocation and takes
them from the NUMA node of that device. On other devices, the pages are process
memory and follow the Linux memory policy. On a system with GPUs on several NUMA
nodes, a context that contains devices from more than one node would otherwise
place every host allocation on the node of its first device.

On Linux, the driver selects the device that creates a host USM allocation from
the NUMA node of the calling thread. On devices that report memory regions, the
pages then come from the node of the selected device. The placement is best
effort: the kernel driver can take pages from another node, for example when the
selected node has no free memory.

# Requirements

The driver selects the device only when all of these conditions are true:

* The operating system is Linux.
* The context contains two or more root devices.
* The root devices of the process are on two or more NUMA nodes, as reported by
  the `numa_node` sysfs attribute of each device.
* `libnuma.so.1` can be loaded.
* The application does not give its own host pointer for the allocation.

Otherwise, the first device of the context creates the allocation.

With one device per process, for example with `ZE_AFFINITY_MASK`, that device
creates every host allocation, and on devices that report memory regions the
pages come from its node.

# Node selection

The driver reads the memory policy of the calling thread at each allocation:

* A policy `MPOL_BIND`, `MPOL_PREFERRED` or `MPOL_PREFERRED_MANY` with exactly
  one node in its node mask selects that node.
* A policy `MPOL_DEFAULT`, `MPOL_LOCAL`, or `MPOL_PREFERRED` with an empty node
  mask selects the node of the CPU affinity of the thread, when that affinity
  covers exactly one node.
* Any other case keeps the first device of the context: a policy with more than
  one node, `MPOL_INTERLEAVE`, `MPOL_WEIGHTED_INTERLEAVE`, a policy with
  `MPOL_F_RELATIVE_NODES`, or a CPU affinity over more than one node.

# Device selection

For the selected node, the driver creates the allocation through the device of
the context on that node. When the node has no device of the context, the
driver uses the device with the smallest `numa_distance` to the node. When two
devices have the same distance, the first one in context order is used. This
selection skips devices with no known node (`numa_node` is -1). When it finds
no device, the first device of the context creates the allocation.

# Binding the allocating thread

Bind the thread before it calls `zeMemAllocHost` or `clHostMemAllocINTEL`.
Binding a memory range after the allocation, for example with `mbind` on the
returned pointer, does not move the pages.

Examples:

```
numactl --cpunodebind=1 ./app      # CPU affinity on node 1
numactl --membind=1 ./app          # memory policy on node 1
taskset -c <cpus of node 1> ./app  # CPU affinity on node 1
```

In a process that allocates for several nodes, set the CPU affinity of each
allocating thread with `pthread_setaffinity_np`, or its memory policy with
`set_mempolicy`, before the allocation.

# Debug keys

* `EnableNumaAwareHostUsmPlacement`: -1 (default) enables the device selection,
  0 disables it, 1 enables it.
