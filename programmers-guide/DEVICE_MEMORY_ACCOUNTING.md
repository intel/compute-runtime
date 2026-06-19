<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Device Memory Accounting and Reservations

* [Introduction](#Introduction)
* [The layered memory model](#The-layered-memory-model)
* [Reservation types](#Reservation-types)
* [Querying memory at runtime](#Querying-memory-at-runtime)
* [Kernel driver differences and small BAR](#Kernel-driver-differences-and-small-BAR)
* [Itemizing UMD allocations (Debug Keys)](#Itemizing-UMD-allocations-Debug-Keys)
* [Recommended measurement procedure](#Recommended-measurement-procedure)

# Introduction

The amount of device-local memory that an application can allocate is smaller than the
physical capacity of the silicon (the HBM/GDDR datasheet figure). The capacity is reduced
in layers by three different components - the GPU firmware and kernel-mode driver (KMD),
then the user-mode driver (UMD, i.e. Level Zero / OpenCL), and finally by whatever is
currently allocated on the device.

Most of these reservations are already subtracted before the value ever reaches the
application, which is why there is no single API that enumerates every reservation. This
document explains the layers, the APIs that expose each layer, and the debug keys that
itemize the UMD reservation - information that is useful for device-memory capacity
planning (for example, sizing workloads such as inference servers on a given SKU).

# The layered memory model

Device-local memory passes through four layers, each of which subtracts a reservation:

```
[1] Physical silicon capacity (HBM/GDDR datasheet capacity)
        |  minus KMD + GPU firmware reservations:
        |    page tables (PPGTT), GuC/HuC firmware images and logs,
        |    WOPCM, migration/eviction scratch, ECC carveout (when enabled)
        v
[2] KMD/OS usable size   (Linux Xe: drm_xe_mem_region.total_size; Linux i915: probed_size;
        |                 Windows: WDDM dedicated video memory)
        |  minus UMD headroom (a fraction held back for the driver's own heaps)
        v
[3] UMD-advertised total (ze_device_memory_properties_t.totalSize / CL global mem size)
        |  minus currently allocated memory (accounted across all processes)
        v
[4] Free / usable right now (zesMemoryGetState().free; currUsableMemSize extension)
```

The value that each layer corresponds to, and the API that exposes it:

| Layer | Value | How to read it |
|-------|-------|----------------|
| [1] | Raw silicon capacity | Datasheet, or platform telemetry (PMT). Not reliably exposed through Level Zero on the Xe KMD - see [Kernel driver differences](#Kernel-driver-differences-and-small-BAR). |
| [2] | KMD usable total (per memory module) | `zesMemoryGetState().size` |
| [2] | Reported physical size | `zesMemoryGetProperties().physicalSize` (equals usable size on Xe; true physical capacity on Windows and i915-prelim) |
| [3] | UMD-advertised total | `zeDeviceGetMemoryProperties().totalSize` (OpenCL: global memory size) |
| [4] | Currently free | `zesMemoryGetState().free` |
| [4] | Live usable right now | `currUsableMemSize` (usable-memory extension on `zeDeviceGetProperties`, see below) |

There is no single "reserved memory list" because the reservations live in different
components and most are netted out before the UMD sees them. The gap between two adjacent
layers is the reservation applied at that layer.

# Reservation types

## KMD and GPU firmware

Fixed reservations applied by the kernel-mode driver and GPU firmware - for example page
tables (PPGTT), the GuC and HuC firmware images and their logs, WOPCM, and
migration/eviction scratch. These are *already removed* from the size the driver reports as
device memory. They are not individually enumerable through a runtime API; their combined
effect is observable as the gap between the datasheet capacity (layer 1) and the KMD usable
size (layer 2, `zesMemoryGetState().size`).

## ECC

When ECC is enabled, the memory controller reserves a fixed fraction of capacity for
error-correction syndrome bits (the exact fraction is platform-specific). ECC is a mode
that can be queried and toggled through the sysman ECC API:

* `zesDeviceEccAvailable`
* `zesDeviceEccConfigurable`
* `zesDeviceGetEccState`
* `zesDeviceSetEccState`

Changing the ECC mode is not immediate. After `zesDeviceSetEccState`, the sysman state
reports a `pendingState` and a `pendingAction` (typically `ZES_DEVICE_ACTION_WARM_CARD_RESET`)
that must be performed before the new mode becomes active. Once active, the reduced capacity
is what `zesMemoryGetState().size` reports - the application does not need to subtract the
ECC carveout itself.

## UMD headroom

On discrete GPUs the driver advertises only a fraction of the KMD-usable size as the
device total (`ze_device_memory_properties_t.totalSize`), holding the remainder back for
its own internal allocations - command buffers, ring and semaphore buffers, kernel ISA,
internal heaps, and debug surfaces. The fraction is product- and OS-specific:

| Path | Discrete (local memory) | Integrated (system memory) |
|------|-------------------------|----------------------------|
| Linux / DRM | 0.95 | 0.94 |
| Windows / WDDM | 0.98 | 0.94 |

The headroom factor can be overridden for experimentation with the
`ClDeviceGlobalMemSizeAvailablePercent` debug key. On integrated devices (no local
memory), a fixed additional internal-resources amount is also subtracted.

## Variation by device category

The ECC fraction, the firmware footprint, and small-BAR versus full-BAR behavior all vary
by product and by kernel driver (i915 versus Xe). The recommendation is to query these
values at runtime on each SKU rather than hardcoding constants.

# Querying memory at runtime

* `zesMemoryGetState()` returns, per memory module, `.size` (the KMD-usable total) and
  `.free` (currently unallocated). This is the most direct "how much is really there / how
  much is left" signal.
* `zeDeviceGetMemoryProperties()` returns `.totalSize`, the UMD-advertised total (i.e.
  after the headroom fraction above).
* The **usable-memory extension** returns the live usable figure in a single Level Zero
  core call, without a sysman dependency. Chain
  `ze_device_usablemem_size_ext_properties_t`
  (`ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES`) onto the `pNext` of
  `ze_device_properties_t` and read `currUsableMemSize` after `zeDeviceGetProperties`. This
  value reflects memory used across the whole device (not just the calling process), which
  is what is needed for capacity planning on a shared device. Confirm the extension is
  supported first by looking for `ZE_DEVICE_USABLEMEM_SIZE_PROPERTIES_EXT_NAME` in
  `zeDriverGetExtensionProperties`.

```cpp
// Confirm support via zeDriverGetExtensionProperties (name: ZE_DEVICE_USABLEMEM_SIZE_PROPERTIES_EXT_NAME).
ze_device_usablemem_size_ext_properties_t usableMem = {ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES};
usableMem.pNext = nullptr;

ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
deviceProperties.pNext = &usableMem;

zeDeviceGetProperties(hDevice, &deviceProperties);
// usableMem.currUsableMemSize -> live usable right now, device-wide (layer 4)
```

For the UMD-advertised total (layer 3), read `totalSize` from `zeDeviceGetMemoryProperties`.
A complete, runnable example is in
[zello_usablemem_properties.cpp](../level_zero/core/test/black_box_tests/zello_usablemem_properties.cpp).

# Kernel driver differences and small BAR

Reading "physical minus usable" to obtain the KMD reservation does **not** work from
Level Zero alone on the Xe KMD:

* `drm_xe_mem_region.total_size` is documented as *"The usable size in bytes for this
  region"* - it is already net of the KMD, firmware and ECC reservations.
* On Xe, `zesMemoryGetProperties().physicalSize` also returns this usable size, so
  `physicalSize == zesMemoryGetState().size`. The raw silicon capacity is not exposed
  through Level Zero on Xe; use the datasheet or platform telemetry (PMT) as the reference
  for layer 1.
* On the older i915-prelim KMD, `physicalSize` is sourced separately and does report the
  true physical capacity (on sub-devices), so there the difference is meaningful.

**Small BAR:** `drm_xe_mem_region.cpu_visible_size` is always `<= total_size`. On small-BAR
parts the CPU-visible portion of device memory is smaller than the total; on full-BAR
systems the two are equal. This matters when device memory is mapped on the host.

**Windows (WDDM):** on Windows the kernel-mode driver is the WDDM display driver. The UMD
takes the device-memory size from WDDM (dedicated video memory) and applies the same
headroom factor as on Linux (0.98 discrete, 0.94 integrated). Unlike the Xe KMD,
`zesMemoryGetProperties().physicalSize` on Windows reports the true physical capacity (the
installed system memory on integrated parts), so the layer 1 -> 2 "physical minus usable"
calculation is meaningful there. `zesMemoryGetState()` reports `.size` (the KMD-reported
total) and `.free` (that size minus what is currently used), and the `currUsableMemSize`
extension is available as on Linux.

# Itemizing UMD allocations (Debug Keys)

To answer "what does the UMD reserve on the device, by type", NEO has a built-in,
aggregated allocation summary. These are debug-settings keys, so they require a
debug-enabled driver build (or the productized debug-key path); set them via an
environment variable or `igdrcl.config`.

## Aggregated summary (recommended for footprint questions)

```
LogAllocationSummaryReport=1
PrintMemoryRegionSizes=1
```

`LogAllocationSummaryReport=1` prints, at process exit, each allocation type with its total
bytes and its percentage of total local/system memory, sorted by size, plus a peak live
memory section. `PrintMemoryRegionSizes=1` prints each memory bank (type, instance, size)
so the summary totals can be anchored against the KMD-reported region size.

Sample output (only sections that contain allocations are printed):

```
=== Allocation Summary Report ===
System Memory Allocations (total: <bytes> bytes):
  <ALLOCATION_TYPE>                          <bytes> bytes  ( xx.xx%)
  ...
Local Memory Allocations (total: <bytes> bytes):
  COMMAND_BUFFER                             <bytes> bytes  ( xx.xx%)
  KERNEL_ISA                                 <bytes> bytes  ( xx.xx%)
  RING_BUFFER                                <bytes> bytes  ( xx.xx%)
  ...
=== Peak Live Memory ===
Peak System Memory (total: <bytes> bytes):
  ...
Peak Local Memory (total: <bytes> bytes):
  ...
=================================
```

## Per-buffer streaming (for detailed or timing analysis)

```
LogAllocationType=1
LogAllocationStdout=1
LogAllocationMemoryPool=1
PrintBOCreateDestroyResult=1
PrintBOBindingResult=1
PrintIoctlEntries=1
```

These emit one line per buffer object as it is created, bound and destroyed, with its type
and size. This view is best for host-overhead and timing analysis; answering a footprint
question from it requires manual aggregation, which is what the summary report above does
for you.

# Recommended measurement procedure

1. Call `zesMemoryGetState()` per memory module - the authoritative usable total (`.size`)
   and currently free (`.free`).
2. Compare `.size` against the datasheet HBM/GDDR capacity - that delta is the combined
   KMD + firmware + ECC reservation. On the Xe KMD use the datasheet or PMT capacity as the
   reference, not `physicalSize`.
3. Toggle ECC off and on with the sysman ECC API and re-read `.size` - this isolates the
   ECC carveout.
4. Compare `zeDeviceGetMemoryProperties().totalSize` against `zesMemoryGetState().size` -
   this is the UMD headroom.
5. Set `LogAllocationSummaryReport=1` - this itemizes what fills the UMD reservation, by
   allocation type.
6. For "how much can I allocate right now" in a single call, chain the usable-memory
   extension onto `ze_device_properties_t` and read `currUsableMemSize` from
   `zeDeviceGetProperties`.
