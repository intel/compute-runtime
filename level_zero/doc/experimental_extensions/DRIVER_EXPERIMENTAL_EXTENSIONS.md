<!---

Copyright (C) 2022-2025 Intel Corporation

SPDX-License-Identifier: MIT

-->


# Level Zero GPU Driver Driver Experimental Extensions

## Introduction

The following document describes the driver experimental extensions implemented in the Level Zero Intel(R) GPU driver. These extensions are meant to test and/or gather feedback on interfaces before they could potentially be added Level Zero specification, as well to provide access to functionality specific to Intel(R) GPUs.

Driver extensions may be defined as APIs (interfaces), flags, or environment variables. In the case of interfaces, these shall be accessed through [zeDriverGetExtensionFunctionAddress](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html?highlight=zedrivergetextensionfunctionaddress#_CPPv435zeDriverGetExtensionFunctionAddress18ze_driver_handle_tPKcPPv). Sample code:


```cpp
typedef ze_result_t (*pFnzexMemGetIpcHandles)(ze_context_handle_t, const void *, uint32_t *, ze_ipc_mem_handle_t *);
...

pFnzexMemGetIpcHandles zexMemOpenIpcHandlePointer = nullptr;
ze_result_t res = zeDriverGetExtensionFunctionAddress(hDriver, "zexMemOpenIpcHandles", reinterpret_cast<void **>(&zexMemOpenIpcHandlePointer)));
```

### [Multiple IPC Handles](MULTIPLE_IPC_HANDLES.md)
### [Multi-CCS Modes](MULTI_CCS_MODES.md)
### [Local memory allocation mode](LOCAL_MEMORY_ALLOCATION_MODE.md)