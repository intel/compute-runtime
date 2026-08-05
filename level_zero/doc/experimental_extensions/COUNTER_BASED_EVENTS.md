<!---

Copyright (C) 2024-2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Counter Based Events

> **DEPRECATED.** Counter Based Events are part of the official Level Zero specification
> since **version 1.15**. New code must use the core API (`zeEventCounterBasedCreate` and
> the related `ze*` entry points) described in the
> [Counter Based Events](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#counter-based-events)
> chapter of the Level Zero programming guide.
>
> The `ZEX_counter_based_event` extension described here is kept only for backwards
> compatibility. Its entry points are thin wrappers that map onto the core API, and no
> new functionality will be added to them. See [Migration](#Migration) for the mapping.

* [Migration](#Migration)
* [Overview](#Overview)
* [Creation](#Creation)
* [External sync allocation](#External-sync-allocation)
* [Aggregated event](#Aggregated-event)
* [Obtaining counter memory and value](#Obtaining-counter-memory-and-value)
* [IPC sharing](#IPC-sharing)
* [Regular command list](#Regular-command-list)
* [Multi directional dependencies on Regular command lists](#Multi-directional-dependencies-on-Regular-command-lists)

# Migration

| Deprecated extension API | Official core API |
| --- | --- |
| `zexCounterBasedEventCreate2` | `zeEventCounterBasedCreate` |
| `zexCounterBasedEventCreate` | `zeEventCounterBasedCreate` |
| `zexEventGetDeviceAddress` | `zeEventCounterBasedGetDeviceAddress` |
| `zexCounterBasedEventGetIpcHandle` | `zeEventCounterBasedGetIpcHandle` |
| `zexCounterBasedEventOpenIpcHandle` | `zeEventCounterBasedOpenIpcHandle` |
| `zexCounterBasedEventCloseIpcHandle` | `zeEventCounterBasedCloseIpcHandle` |
| `zexDeviceGetAggregatedCopyOffloadIncrementValue` | `zeDeviceGetAggregatedCopyOffloadIncrementValue` |
| `zex_counter_based_event_desc_t` | `ze_event_counter_based_desc_t` |
| `zex_counter_based_event_external_sync_alloc_properties_t` | `ze_event_counter_based_external_sync_allocation_desc_t` |
| `zex_counter_based_event_external_storage_properties_t` | `ze_event_counter_based_external_aggregate_storage_desc_t` |
| `zex_ipc_counter_based_event_handle_t` | `ze_ipc_event_counter_based_handle_t` |
| `ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE` / `_NON_IMMEDIATE` / `_HOST_VISIBLE` / `_IPC` | `ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE` / `_NON_IMMEDIATE` / `_HOST_VISIBLE` / `_IPC` |
| `ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP` | `ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP` |
| `ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP` | `ZE_EVENT_COUNTER_BASED_FLAG_HOST_TIMESTAMP` |
| `ZEX_COUNTER_BASED_EVENT_FLAG_EXTERNAL` | `ZE_EVENT_COUNTER_BASED_FLAG_GRAPH_EXTERNAL` |
| `zex_intel_event_sync_mode_exp_desc_t` | `ze_event_sync_mode_desc_t` |
| `ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC` | `ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC` |
| `ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES` | `ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_SYNC_ALLOCATION_DESC` |
| `ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES` | `ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_EXTERNAL_AGGREGATE_STORAGE_DESC` |
| `ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC` | `ZE_STRUCTURE_TYPE_EVENT_SYNC_MODE_DESC` |

Note that neither the flag names nor the `stype` enumerators are a mechanical `ZEX_` -> `ZE_`
substitution: the two timestamp flags and the external/graph flag were renamed, and the
`stype` names reorder the words. The flags keep the same bit values, which is why the
deprecated entry points can pass them through unchanged.

Creating Counter Based Events through an Event Pool - `zeEventCreate` on a pool created
with the `ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_*` flags - was possible through the separate
`ZE_experimental_event_pool_counter_based` extension. That extension is deprecated since
1.15 as well, and is also replaced by `zeEventCounterBasedCreate`.

The following core API has no equivalent in the deprecated extension:

* `zeEventGetCounterBasedFlags` - returns the flags used to create a Counter Based Event
  (`0` for regular Events created with `zeEventCreate`). Requires Level Zero API version 1.17.
  **This driver currently reports only the immediate / non-immediate flags** - `HOST_VISIBLE`,
  `IPC`, the timestamp flags and `GRAPH_EXTERNAL` are not returned.
* `zeDeviceGetCounterBasedEventMaxValue` - returns the maximum value supported for
  externally managed counter storage. Requires Level Zero API version 1.17.
  **Not implemented in this driver yet** - it currently returns
  `ZE_RESULT_ERROR_UNSUPPORTED_FEATURE`.

The rest of this document describes the behavior of Counter Based Events as implemented
by this driver. It applies equally to the core API and to the deprecated extension.

# Overview

This type of event, referred to as a Counter Based (CB) Event, does not require an event pool, as the related allocations are managed internally by the driver. This reduces the overhead on the host for managing pool allocations.  
The CB Event can only be signaled on the GPU using an in-order command list.  

Every in-order command list has an internal submission counter that is updated with each append call. This counter manages internal in-order dependencies. The next append call waits for that counter, for example, using the `MI_SEMAPHORE_WAIT` GPU command.  
Note that some operations may be optimized, and the counter value may not directly correspond to the number of append calls.  

When a CB Event is passed as a signal event, it points to a specific counter value and memory location. Since the command list manages the counter allocation, this method avoids producing additional GPU memory operations (except timestamps). As a result, users do not need to explicitly control event completion before reusing it.  

**Key features:**
- After creation, a CB Event is initially marked as completed. The completion state changes if the event is assigned as a `signalEvent` to an append call or if external storage is specified during creation.
- CB Event can be waited for from any command list type.
- `zeEventHostReset` is not allowed. Can be reused on any in-order command list without explicit reset. A new API call just replaces its previous state (counter/allocation)
- `zeEventHostSignal` is not allowed. Can be signaled only from in-order command list
- No need to wait for completion before reusing/destroying
- CB Event doesn't own any memory allocations. Can be reused/destroyed with low cost. Timestamp allocation is also handled internally by the Driver
- Device association is not fixed at creation. Each signal operation re-associates the Event with the device of the signaling command list. Waiting does not re-associate it, but requires P2P access to the device that last signaled it
- IPC sharing is bi-directional. The Event opened in another process can be used for waiting, querying and signaling, and both processes observe the latest state without repeating the exchange. See [IPC sharing](#IPC-sharing)
- Regular command list (known as recorded or non-immediate) is a special use case for CB Events. Will be described in separate section

Regular Event rely on memory state controlled by the User (explicit Reset calls). CB Event represents host programming sequence, without managing the state. For example:
 
```cpp
zeEventCounterBasedCreate(context, device, &desc, &event1); // counter not yet assigned

zeCommandListAppendLaunchKernel(cmdList1, kernel, &groupCount, event1, 0, nullptr); // assigned counter=X on memory CL1_alloc
zeCommandListAppendLaunchKernel(cmdList2, kernel, &groupCount, nullptr, 1, &event1); // cmdList2 waits for counter=X on memory CL1_alloc

// reuse without waiting/reset
zeCommandListAppendLaunchKernel(cmdList3, kernel, &groupCount, event1, 0, nullptr); // Replace state. Assigned counter=Y on memory CL3_alloc

// Event1 is implicitly reset to different state.
// cmdList2 can be still running on GPU. It waits for counter=X on memory CL1_alloc. 
// Its also safe to delete Event object.

zeEventHostSynchronize(event1, UINT64_MAX); // wait for counter=Y on memory CL3_alloc
```

# Creation

```cpp
ze_result_t zeEventCounterBasedCreate(
                ze_context_handle_t hContext,
                ze_device_handle_t hDevice,
                const ze_event_counter_based_desc_t *desc,
                ze_event_handle_t *phEvent);
```

`ze_event_counter_based_desc_t` defines the Event flags and the signal/wait scopes. Storage
type and synchronization mode are selected by chaining extension structures on `pNext`.

Flags below are listed without their `ZE_EVENT_COUNTER_BASED_FLAG_` prefix.

| Flag (`ze_event_counter_based_flag_t`) | Description |
| --- | --- |
| `IMMEDIATE` | Event can be used on immediate command lists |
| `NON_IMMEDIATE` | Event can be used on regular (non-immediate) command lists |
| `HOST_VISIBLE` | Signals and waits are also visible to host |
| `IPC` | Event can be shared across processes |
| `DEVICE_TIMESTAMP` | Event contains timestamps in the device time domain |
| `HOST_TIMESTAMP` | Event contains timestamps converted to the host time domain. Cannot be combined with `DEVICE_TIMESTAMP` |
| `GRAPH_EXTERNAL` | Event recorded in a graph or in a cloned command list can also be used outside of it (external wait, host synchronization). Requires Level Zero API version 1.17 |

Notes:
- If neither `IMMEDIATE` nor `NON_IMMEDIATE` is set, `IMMEDIATE` is assumed.
- `IPC` combined with `DEVICE_TIMESTAMP` or `HOST_TIMESTAMP` returns `ZE_RESULT_ERROR_INVALID_ARGUMENT`.

`zeEventCounterBasedCreate` is the only API that should be used to create a Counter Based
Event. There were multiple earlier attempts to define the creation API. All of them are
**deprecated**, and all of them are still accepted by the driver:

### 1. 
```cpp
ze_result_t zexCounterBasedEventCreate(  
                ze_context_handle_t hContext,  
                ze_device_handle_t hDevice,
                uint64_t *deviceAddress,
                uint64_t *hostAddress,
                uint64_t completionValue,
                const ze_event_desc_t *desc,
                ze_event_handle_t *phEvent);
```  

### 2. 
Existing `zeEventCreate`, on a pool created with the Event Pool flags
`ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE` / `ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE`
(the separately deprecated `ZE_experimental_event_pool_counter_based` extension). Events
created this way are full Counter Based Events, but this path cannot express the newer
parameters (external storage, sync mode) and cannot be combined with
`ZE_EVENT_POOL_FLAG_IPC`.

### 3. 
```cpp
ze_result_t zexCounterBasedEventCreate2(
                ze_context_handle_t hContext, 
                ze_device_handle_t hDevice, 
                const zex_counter_based_event_desc_t *desc, 
                ze_event_handle_t *phEvent);
```

# External sync allocation
User may optionally specify externally managed counter allocation and value. This can be done by passing `ze_event_counter_based_external_sync_allocation_desc_t` as extension of `ze_event_counter_based_desc_t`.  

**Requirements:**
- Counter allocation is managed by the User. May be allocated outside L0 Driver. Some limitations may be expected
- User must ensure device allocation (`deviceAddress`) residency. It must be accessible by GPU
- Host allocation (`hostAddress`) must be accessible by CPU (eg. waiting for completion)
- User is responsible for updating both memory locations to >= `completionValue` to signal Event completion
- Signaling such event, replaces the state (as described previously)
- `completionValue`, and every value written by the User to `deviceAddress` / `hostAddress`, must not exceed the value returned by `zeDeviceGetCounterBasedEventMaxValue`. The Driver does not validate values written by the User

# Aggregated event
Aggregated event is a special use case for CB Events. It can be signaled from multiple append calls, but waiting requires only one memory compare operation.  
It can be created by passing `ze_event_counter_based_external_aggregate_storage_desc_t` as extension of `ze_event_counter_based_desc_t`.

**Requirements:**
- This extension cannot be used with "external sync allocation" extension
- User must ensure device allocation (`deviceAddress`) residency. It must be accessible by GPU. It must resolve to an allocation known to the Driver, otherwise creation returns `ZE_RESULT_ERROR_INVALID_ARGUMENT`
- `incrementValue` must be greater than 0. Creation with `incrementValue == 0` returns `ZE_RESULT_ERROR_INVALID_ARGUMENT`
- Driver will use `deviceAddress` for host synchronization as USM allocation. It must be accessible by CPU. If the Driver cannot lock it for CPU access, host waits are not possible
- Apart from the signaling operation, the Driver does not write to that memory. Initial value and any reset are fully under the User's control
- Signaling such event, will not replace its state (as described previously). It can be passed to multiple append calls and each append will increment the storage by `incrementValue` (atomically) on GPU
- When signaling append calls originate on more than one device, the increments are cross-device atomics. User must ensure that atomics are supported between the involved devices (`ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS` returned by `zeDeviceGetP2PProperties`)
- Using aggregated event as dependency, requires only one memory compare operation against final value: `*deviceAddress` >= `completionValue`
- Profiling is not possible if producers originate on different GPUs (different timestamp domains)
- A single append call may be distributed by the Driver over multiple engines (for example copy offload split), so the number of increments per append is not defined by the API. `zeDeviceGetAggregatedCopyOffloadIncrementValue` returns a per-device increment value that stays correct regardless of that split
- `completionValue`, and the value aggregated under `deviceAddress` at any point in time, must not exceed the value returned by `zeDeviceGetCounterBasedEventMaxValue`

# Obtaining counter memory and value
User may obtain counter memory location and value. For example, waiting for completion outside the L0 Driver.  
If Event state is replaced by new append call or `zeCommandQueueExecuteCommandLists` that signals such Event, below API must be called again to obtain new data.

```cpp
ze_result_t zeEventCounterBasedGetDeviceAddress(
                ze_event_handle_t hEvent,
                uint64_t *completionValue,
                uint64_t *deviceAddress);
```

# IPC sharing

IPC sharing is bi-directional. The Event opened in the second process refers to the same synchronization point as the original Event. It can be used for:
- waiting and querying completion, on host and on GPU
- signaling from an in-order command list created in that process

Signaling a CB Event replaces its state (counter value and memory location). For a shared Event, the new state is propagated to the other process:

- Both processes observe the latest state, regardless of which process performed the signal operation
- IPC handle obtained with `zeEventCounterBasedGetIpcHandle` remains valid after the state is replaced. It doesn't have to be obtained again
- Event opened with `zeEventCounterBasedOpenIpcHandle` remains valid after the state is replaced. It doesn't have to be opened again and stays usable until `zeEventCounterBasedCloseIpcHandle` is called
- There is still no need to wait for completion before replacing the state or destroying the Event object, in any of the processes

An Event may also be shared before it is signaled for the first time. In that case no state is assigned yet, and the first signal operation (performed by any of the processes) defines the synchronization point observed by both of them.

Since the state is shared, ordering of cross-process operations is the User's responsibility. Waiting for such Event resolves the synchronization point that is known at the time of the wait/append call.

**Timestamps are not allowed for IPC sharing.**

```cpp
// process 1
desc.flags |= ZE_EVENT_COUNTER_BASED_FLAG_IPC;
zeEventCounterBasedCreate(context, device, &desc, &event1);

zeEventCounterBasedGetIpcHandle(event1, &ipcHandle); // may be obtained before the first signal (bi-directional sharing only, see "Driver support")

// process 2
zeEventCounterBasedOpenIpcHandle(context2, ipcHandle, &event2); // opened only once

// process 1
zeCommandListAppendLaunchKernel(cmdList1, kernel, &groupCount, event1, 0, nullptr); // assigned counter=X on memory CL1_alloc

// process 2
zeCommandListAppendWaitOnEvents(cmdList2, 1, &event2); // waits for counter=X on memory CL1_alloc

// process 1
zeCommandListAppendLaunchKernel(cmdList3, kernel, &groupCount, event1, 0, nullptr); // Replace state. counter=Y on memory CL3_alloc

// process 2
zeCommandListAppendWaitOnEvents(cmdList2, 1, &event2); // waits for counter=Y on memory CL3_alloc. No need to open the handle again
zeCommandListAppendLaunchKernel(cmdList2, kernel, &groupCount, event2, 0, nullptr); // process 2 may also signal it

// process 2
zeEventCounterBasedCloseIpcHandle(event2); // Free if not needed
```

**Driver support:**

Bi-directional sharing requires OS support for transferring an allocation handle between
live processes. On Windows this is always available (NT handles). On Linux the Driver uses
one of two mechanisms, selected once when the Context is created: the `pidfd_open` /
`pidfd_getfd` syscalls, or a Unix socket handle server when those syscalls are unavailable.
The two are mutually exclusive - a `pidfd_getfd` denial at open time (for example Yama
`ptrace_scope`, or a container without `CAP_SYS_PTRACE`) is reported as an error from
`zeEventCounterBasedOpenIpcHandle` and is not retried over the socket server.

When neither mechanism is available, the Driver falls back to the legacy one-directional
sharing. In that mode the opened Event is a snapshot that can only be used for
waiting/querying, a new IPC exchange is required after every state change, and
`zeEventCounterBasedGetIpcHandle` returns `ZE_RESULT_ERROR_INVALID_ARGUMENT` if the Event
has not been assigned a state yet - the Event must be signaled before it can be shared.

### IPC handle creation
To share a CB Event across processes, an IPC handle can be obtained as follows:
```cpp
ze_result_t zeEventCounterBasedGetIpcHandle(ze_event_handle_t hEvent, ze_ipc_event_counter_based_handle_t *phIpc);
```
* `hEvent`: Handle of the event object.
* `phIpc`: Pointer to the IPC handle to be populated.

### IPC handle opening
In a different process, the IPC handle can be opened to create a new event object that points to the same synchronization point:
```cpp
ze_result_t zeEventCounterBasedOpenIpcHandle(ze_context_handle_t hContext, ze_ipc_event_counter_based_handle_t hIpc, ze_event_handle_t *phEvent);
```

`hContext` does not have to match the context used to create the Event in the exporting
process. Any context of the importing process may be used, including the default one.

### IPC handle closing
Once the IPC handle is no longer needed, new Event can be closed:
```cpp
ze_result_t zeEventCounterBasedCloseIpcHandle(ze_event_handle_t hEvent);
```

# Regular command list
Regular command list is a special use case for CB Events. Counter state is additionally reset on every `zeCommandQueueExecuteCommandLists` call.  
Any API call that relies on explicit counter memory/value (eg. `zeEventCounterBasedGetDeviceAddress`) needs to be called again to obtain new data.  
Other API calls that don't specify counter explicitly, are managed by the Driver.  

**Each regular command list execution updates state of the events that will be signaled in that command list to "not ready".**  
**This rule applies to `zeCommandQueueExecuteCommandLists` and `zeCommandListImmediateAppendCommandLists` API calls.**  
**Internal command list counter is implicitly reset to 0 on every execution.**  
**L0 doesn't allow to execute the same regular command list multiple times in parallel. It must be synchronized before next execution.**  
**Any waiting operation on such Event, must ensure that it is waiting for the correct `zeCommandQueueExecuteCommandLists` call.**  

```cpp
// in-order operations
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, nullptr, 0, nullptr);
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, event1, 0, nullptr); // signalEvent on 2nd operation
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, nullptr, 0, nullptr);
zeCommandListClose(regularCmdList1);

zeCommandQueueExecuteCommandLists(cmdQueue1, 1, &regularCmdList1, nullptr); // Command list has 3 operations. Counter updated to {1->2->3}
zeCommandQueueSynchronize(cmdQueue1, timeout);
zeCommandQueueExecuteCommandLists(cmdQueue1, 1, &regularCmdList1, nullptr); // Execute again. Counter implicitly reset to 0. GPU will again increment it to {1->2->3}

// wait from different command list
zeCommandListAppendLaunchKernel(regularCmdList2, kernel, &groupCount, nullptr, 1, &event1); // wait for counter=2 (second operation)
zeCommandListClose(regularCmdList2);
zeCommandQueueExecuteCommandLists(cmdQueue2, 1, &regularCmdList2, nullptr); // wait for counter=2, second execution of regularCmdList1
zeCommandQueueSynchronize(cmdQueue2, timeout);

// execute regularCmdList1 3rd time
zeCommandQueueExecuteCommandLists(cmdQueue1, 1, &regularCmdList1, nullptr); // Counter reset and again updated to {1->2->3}

// execute regularCmdList2 2nd time
zeCommandQueueExecuteCommandLists(cmdQueue2, 1, &regularCmdList2, nullptr); // wait for counter=2, 3rd execution of regularCmdList1
zeCommandQueueSynchronize(cmdQueue2, timeout);
```

# Multi directional dependencies on Regular command lists
Regular command list with overlapping dependencies may be executed multiple times. For example, two command lists are executed in parallel with bi-directional dependencies.  
Its important to understand counter (Event) state transition, to correctly reflect Users intention.  

```
regularCmdList1:       (A)      ------------->   (wait for B)   ----->   (C)
                        |                            ^
                        |                            |
                        V                            |
regularCmdList2:   (wait for A)  ------------->     (B)         ----->   (D)
```

In this example, all Events are synchronized to "ready" state after the first execution. 
It means that second execution of `regularCmdList1` waits again for "ready" `{1->2->3}` state of `regularCmdList2` (first execution) instead of `{4->5->6}`.  
This is because `regularCmdList2` was not yet executed for the second time. And their counters were not updated.

**First execution:**
```cpp
// All Events are in "not ready" state
zeCommandQueueExecuteCommandLists(cmdQueue1, 1, &regularCmdList1, nullptr); // Counter updated to {1->2->3}
zeCommandQueueExecuteCommandLists(cmdQueue2, 1, &regularCmdList2, nullptr); // Counter updated to {1->2->3}

// All Events are "ready" now
zeCommandQueueSynchronize(cmdQueue1, timeout); // wait for counter=3
zeCommandQueueSynchronize(cmdQueue2, timeout); // wait for counter=3
```

**Second execution:**
```cpp
// regularCmdList1 waits for "ready" {1->2->3} Events from first execution of regularCmdList2
// regularCmdList1 changes Events state to "not ready"
zeCommandQueueExecuteCommandLists(cmdQueue1, 1, &regularCmdList1, nullptr); // Counter updated to {4->5->6}

// regularCmdList2 waits for "not ready" {4->5->6} Events from second execution of regularCmdList1
zeCommandQueueExecuteCommandLists(cmdQueue2, 1, &regularCmdList2, nullptr); // Counter updated to {4->5->6}
```

**Different approach:**

To avoid above situation, User must remove all bi-directional dependencies. By using single command list (if possible) or split the workload into different command lists with single-directional dependencies.  

Using Counter Based Events for such scenarios is not always the most optimal usage mode. It may be better to use Regular Events with explicit Reset calls.
