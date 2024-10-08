<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Module Symbols and Linking in Level Zero

- [Module Symbols and Linking in Level Zero](#module-symbols-and-linking-in-level-zero)
- [Introduction](#introduction)
- [Intel Graphics Compiler Build Flags](#intel-graphics-compiler-build-flags)
- [Function Pointers](#function-pointers)
- [Global Variables](#global-variables)
- [Dynamic Module Linking](#dynamic-module-linking)
- [Static Module Linking](#static-module-linking)
- [Overall Limitations](#overall-limitations)

# Introduction

Modules used by Level Zero can be generated with Exported/Imported Symbols such as:
* Global Variables
* Functions

These symbols are generated usually through a SPIRv which has decorations here `https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_linkage`

With one of the following linkage types `https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_linkage_type`

When SPIRv binaries or oneAPI DPC++ binaries are compiled by the Intel Graphics Compiler, these symbols will be processed by IGC assuming the proper flags described below are passed as `build flags`.

# Intel Graphics Compiler Build Flags
To enable the ability for a Module to export Symbols, the Intel Graphics Compiler must be passed one or both of the following build flags in `zeModuleCreate`:
- `-ze-take-global-address` : Enables IGC to export Global Variables as symbols.
- `-library-compilation` : Enables IGC to export Functions other than kernels as symbols.

# Function Pointers
Function Pointers are supported in Level Zero through the API `https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=functionpointer#_CPPv426zeModuleGetFunctionPointer18ze_module_handle_tPKcPPv`

Function pointers refer to non-kernel functions reported as an `export` symbol in the module.

In addition, by OpenCL standards, function pointers usually are not allowed in kernels. For Level Zero support is enabled through a SPIRv extension `SPV_INTEL_function_pointers` and if using a .cl kernel, through an OpenCL extension `__cl_clang_function_pointers`.

To compile a .cl kernel into a SPIRv that has function pointers requires use of these extensions.

.cl kernel:

```
    #pragma OPENCL EXTENSION __cl_clang_function_pointers : enable
```

And during llvm-spirv compile, use of the SPIRv function pointer extension:

`llvm-spirv module_name.bc -o module_name.spv --spirv-ext=+SPV_INTEL_function_pointers`

# Global Variables
Global Variables in SPIRv Modules are reported and accessed through export symbols when the module is compiled with `-ze-take-global-address`.

The memory address retrieved from the API `https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=getglobal#_CPPv424zeModuleGetGlobalPointer18ze_module_handle_tPKcP6size_tPPv`

Is device-only and must not be read on the host otherwise one will access invalid memory. Only usage of the memory on the device is allowed.

# Dynamic Module Linking
Dynamic Module Linking or Linking L0 Modules to other L0 Modules to resolve imported symbols is enabled through the following L0 APIs:
`https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zemoduledynamiclink`

Please see the functionality described in detail here in the L0 spec: `https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#dynamically-linked-modules`

# Static Module Linking
Static Module Linking or Linking L0 Modules to other L0 Modules during the zeModuleCreate is enabled through the extension:`https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#ze-module-program-exp-desc-t`

This extension to `zeModuleCreate` allows for one to pass more than one Module to Create for the symbol linkage to be resolved into a single large binary.

For the export symbols to be generated by the compiler, one must compile the binary with one or more of the flags in [Intel Graphics Compiler Build Flags](#intel-graphics-compiler-build-flags).

Assuming the Modules used in static link have all the required export symbols, then the resulting Module will no longer contain any unresolved externals that require linkage later with dynamic link and the imported functions will be combined into the final Module.

# Overall Limitations

All features for generating symbols require internal-only flags to enable IGC to process those symbols. Currently IGC does not support generating symbols for SPIRv binaries by default.
