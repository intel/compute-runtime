<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Get Driver Version String

* [Overview](#Overview)
* [Definitions](#Definitions)

# Overview

A new extension API 'zeIntelKernelGetBinaryExp' is created to retrieve both the size of the kernel binary program data, and the data itself.  It can first be called to obtain the size of the kernel binary program data.  The application then allocates memory accordingly and in the second call retrieves the kernel binary program data. 

`zeIntelKernelGetBinaryExp` returns Intel Graphics Assembly (GEN ISA) format binary program data for kernel handle.  The binary program data may be directly disassembled with iga64 offline tool.

# Definitions

```cpp
#define ZE_INTEL_KERNEL_GET_PROGRAM_BINARY_EXP_NAME "ZE_intel_experimental_kernel_get_program_binary"
```

## Interfaces

```cpp
/// @brief Get Kernel Program Binary
///
/// @details
///     - A valid kernel handle must be created with zeKernelCreate.
///     - Returns Intel Graphics Assembly (GEN ISA) format binary program data for kernel handle.
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelKernelGetBinaryExp(
    ze_kernel_handle_t hKernel, ///< [in] Kernel handle
    size_t *pSize,              ///< [in, out] pointer to variable with size of GEN ISA binary
    char *pKernelBinary         ///< [in,out] pointer to storage area for GEN ISA binary function
);
```

## Programming example

```cpp
#include <fstream>

//kernel = valid kernel handle
size_t kBinarySize = 0;
zeKernelGetBinaryExp(kernel, &kBinarySize, nullptr);
char *progArray;
program_array = new char[kBinarySize];
zeKernelGetBinaryExp(kernel, &kBinarySize, progArray);
const std::string file_path = "program_binary.bin";
std::ofstream stream(file_path, std::ios::out | std::ios::binary);
stream.write(reinterpret_cast<const char *>(progArray), kBinarySize);
stream.close();
```
