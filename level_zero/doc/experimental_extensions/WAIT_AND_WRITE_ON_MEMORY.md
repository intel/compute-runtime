<!---

Copyright (C) 2022-2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Wait On Memory and Write To Memory

* [Overview](#Overview)
* [Definitions](#Definitions)

# Overview

Implementation must support `ZE_intel_experimental_command_list_memory_sync` extension.

## Wait On Memory

Wait On Memory provides a low level dependency mechanism through memory locations. It could be used to implement event-like synchronization mechanism where the wait on a memory location is on a value written by a EU thread. A potential use case is to hide context switching latencies between different engine classes.

This functionality is also useful in heterogenous multi-device synchronization setups where, for example, a network card could write to memory location to indicate a packet is ready for consumption and the device waits on memory location via this extension.

Depending on HW, implementation may support different data size to wait (`zexCommandListAppendWaitOnMemory` / `zexCommandListAppendWaitOnMemory64`).

Application must query minimal supported size via `ze_intel_device_command_list_wait_on_memory_data_size_exp_desc_t` struct.  
Not supported API will return error.

## Write To Memory
Write To Memory is a counterpart to the wait on memory method to allow for scenarios where user prefers to write to the memory location from the accelerator device.

The usage models are similar to the Wait on memory functionality in that it could be used to setup cross-engine dependency chain within a device or provide mechanism to setup heterogenous multi-device dependency chains.

# Definitions

```cpp
typedef struct _zex_wait_on_mem_desc_t {
    zex_wait_on_mem_action_flags_t actionFlag;
    zex_mem_action_scope_flags_t waitScope;
} zex_wait_on_mem_desc_t;

typedef struct _zex_write_to_mem_desc_t {
    zex_mem_action_scope_flags_t writeScope;
} zex_write_to_mem_desc_t;
```

## Interfaces

```cpp
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent);

zexCommandListAppendWaitOnMemory64(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint64_t data,
    zex_event_handle_t hSignalEvent);

zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data);
```

## Enums

```cpp
typedef uint32_t zex_mem_action_scope_flags_t;
typedef enum _zex_mem_action_scope_flag_t {
    ZEX_MEM_ACTION_SCOPE_FLAG_SUBDEVICE = ZEX_BIT(0),
    ZEX_MEM_ACTION_SCOPE_FLAG_DEVICE = ZEX_BIT(1),
    ZEX_MEM_ACTION_SCOPE_FLAG_HOST = ZEX_BIT(2),
    ZEX_MEM_ACTION_SCOPE_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_mem_action_scope_flag_t;

typedef uint32_t zex_wait_on_mem_action_flags_t;
typedef enum _zex_wait_on_mem_action_flag_t {
    ZEX_WAIT_ON_MEMORY_FLAG_EQUAL = ZEX_BIT(0),
    ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL = ZEX_BIT(1),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN = ZEX_BIT(2),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL = ZEX_BIT(3),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN = ZEX_BIT(4),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL = ZEX_BIT(5),
    ZEX_WAIT_ON_MEMORY_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_wait_on_mem_action_flag_t;
```

## Programming example

```cpp
// Create a descriptor for wait on mem
zex_wait_on_mem_desc_t waitDesc = {};
waitDesc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
uint32_t initData = 0;

//Initialize memory location
void *zeBuf = nullptr;

//Append Wait On Memory
zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuf);

//Append Wait On Memory
zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendWaitOnMemory", pfnWaitMemFn);
pfnWaitMemFn(cmdList, &waitDesc, zeBuffer, initData, NULL);

zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendWriteToMemory", pfnWriteMemFn);
zex_write_to_mem_desc_t writeDesc = {};
uint32_t signalData = 0x2;
pfnWriteMemFn(cmdList, &writeDesc, zeBuffer, signalData);
```
