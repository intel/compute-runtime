<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Hardware Abstraction

NEO targets many Intel graphics generations from one codebase. This document
explains the mechanism that makes this possible: compile-time specialization on
a `GfxFamily` type, a file-naming convention for version-ranged code, and a set
of runtime helper objects for finer-grained differences. Understanding this
pattern is essential before adding hardware-specific behavior.

For the broader picture of how this fits into the runtime see
[ARCHITECTURE.md](../ARCHITECTURE.md).

* [Graphics families and products](#graphics-families-and-products)
* [Template specialization on GfxFamily](#template-specialization-on-gfxfamily)
* [The .inl file-naming convention](#the-inl-file-naming-convention)
* [Per-family enablement](#per-family-enablement)
* [Runtime helper objects](#runtime-helper-objects)
* [Choosing where a change belongs](#choosing-where-a-change-belongs)

## Graphics families and products

Hardware is organized into **graphics core families** (the `GFXCORE_FAMILY`
enum) and, within a family, concrete **products** (SKUs). Each supported family
has a top-level directory under `shared/source/`, for example:

| Directory | Family |
|-----------|--------|
| `gen12lp/` | Gen12LP |
| `xe_hpg_core/` | Xe HPG |
| `xe_hpc_core/` | Xe HPC |
| `xe2_hpg_core/` | Xe2 HPG |
| `xe3_core/` | Xe3 |
| `xe3p_core/` | Xe3p |

A product is described at runtime by a `HardwareInfo` structure, which combines
a `PLATFORM` (product identity), a `FeatureTable` (capabilities) and related
data. The supported-device tables live under
`shared/source/dll/devices/` and map PCI device IDs to products.

## Template specialization on GfxFamily

Family-independent algorithms are written once as **templates parameterized by a
`GfxFamily` type**. The `GfxFamily` type provides the concrete hardware command
and state structures for that family, so the same algorithm emits the correct
bits for every generation. For example:

```cpp
template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    // ...
};
```

The same approach is used for the command encoders in
`shared/source/command_container/` (`EncodeDispatchKernel<GfxFamily>`,
`EncodeStateBaseAddress<GfxFamily>`, ...), for direct submission, and for the
AUB/TBX simulated receivers.

Because the command structs come from `GfxFamily`, hardware commands must be
initialized from the family-specific constant initializers rather than
zero-initialized by hand, for example:

```cpp
auto pipeControl = GfxFamily::cmdInitPipeControl;
```

Every new hardware command or structure must have a static constant initializer
added to each family that uses it (see [GUIDELINES.md](../GUIDELINES.md)).

## The .inl file-naming convention

A template's declaration lives in a `*_hw.h` header, but its method bodies are
split across `.inl` files whose **suffix encodes the range of families they
apply to**. This lets a single implementation body be shared by every family in
a range, while letting newer families override just the parts that differ.

Common suffix patterns (illustrated with the command stream receiver):

| File | Applies to |
|------|------------|
| `command_stream_receiver_hw_base.inl` | Base behavior for all families. |
| `command_stream_receiver_hw_xehp_and_later.inl` | Xe HP and every later family. |
| `command_stream_receiver_hw_dg2_and_later.inl` | DG2 and later. |
| `command_stream_receiver_hw_from_gen12lp_to_xe_hpg.inl` | A bounded range of families. |
| `command_stream_receiver_hw_from_xe_hpc_to_xe3.inl` | A bounded range of families. |

The same vocabulary (`_base`, `_and_later`, `_from_X_to_Y`, `_and_xe_hpc`, ...)
recurs across the tree - in `command_container/` (the encoders), `helpers/`
(the helper implementations), `direct_submission/`, and the per-family product
and release helpers. When you need to change behavior for a span of
generations, find the `.inl` whose range matches and edit there; when a new
generation must diverge, add a more specific `.inl` for that generation and
later.

> Note: heap vs. heapless addressing is another axis expressed through this
> convention (e.g. `*_heap_addressing.inl` vs. `*_heapless_addressing.inl`).

## Per-family enablement

Each family directory contains thin `.cpp` files that **instantiate the
templates** for that family's concrete `GfxFamily` and register the family with
the runtime. Examples from `shared/source/xe_hpc_core/`:

* `command_stream_receiver_hw_xe_hpc_core.cpp` - instantiates the CSR template.
* `command_encoder_xe_hpc_core.cpp` - instantiates the encoders.
* `enable_family_full_core_xe_hpc_core.cpp` - registers the family so the
  runtime can construct the right objects for a detected device.
* `hw_cmds_*.h` - the family's command/structure definitions and their constant
  initializers.

This separation keeps the heavy template bodies in shared `.inl` files while the
family directories contain only the small amount of code needed to bring a
generation online.

## Runtime helper objects

Not every difference deserves a template specialization. Smaller behavioral and
workaround differences are expressed through helper objects selected at runtime
and stored on the `RootDeviceEnvironment`. Each is constructed for the detected
family/product during initialization.

| Helper | Granularity | Use it for |
|--------|-------------|------------|
| `GfxCoreHelper` | Graphics family (architecture) | Architecture-level queries and command-shaping decisions (scratch sizing, image alignment, default engine type, SIP type, ...). |
| `ProductHelper` | Product / SKU | Product-specific configuration and workarounds (`configureHardwareCustom`, blitter support, page-table-manager support, WA flags). |
| `ReleaseHelper` | IP release | Per-release feature and workaround switches (systolic/MMA support, bindless addressing, GRF configs, mem-bank size). |
| `CompilerProductHelper` | Product (compiler-facing) | Product details the compiler path needs (e.g. supported features advertised to IGC). |
| `AILConfiguration` | Application | Per-application tuning based on the detected process (Application Intelligence Layer). |

These are interfaces with per-family/product implementations. The
implementations frequently reuse the same `.inl` range convention described
above (e.g. `product_helper_from_xe_hpc_to_xe3.inl`,
`gfx_core_helper_xe2_and_later.inl`).

Prefer adding a query to the appropriate helper over scattering
`if (family == ...)` checks through shared algorithms. The helper keeps the
hardware decision in one place and makes the shared code generation-agnostic.

## Choosing where a change belongs

A short decision guide for hardware-specific work:

1. **Different hardware commands/state for a generation** - the change belongs
   in the templated algorithm plus the `.inl` for the relevant family range, and
   may require new constant initializers in the family's `hw_cmds`.
2. **A capability or workaround query (true/false, a size, an enum)** - add it
   to `GfxCoreHelper`, `ProductHelper`, or `ReleaseHelper` depending on whether
   it is architecture-, product-, or release-scoped, and implement it in the
   matching `.inl`.
3. **A compiler-facing detail** - extend `CompilerProductHelper`.
4. **Behavior that depends on the running application** - extend
   `AILConfiguration`.
5. **OS-specific behavior** - that axis is orthogonal to hardware; see the
   operating-system abstraction section in [ARCHITECTURE.md](../ARCHITECTURE.md).

Keeping each kind of difference in its designated place is what allows NEO to
add new generations with minimal churn to existing, validated code.
