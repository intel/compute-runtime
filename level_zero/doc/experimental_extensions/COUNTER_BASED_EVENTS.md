<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Counter Based Events

* [Overview](#Overview)
* [Creation](#Creation)
* [External storage](#External-storage)
* [Aggregated event](#Aggregated-event)
* [Obtaining counter memory and value](#Obtaining-counter-memory-and-value)
* [IPC sharing](#IPC-sharing)
* [Regular command list](#Regular-command-list)
* [Multi directional dependencies on Regular command lists](#Multi-directional-dependencies-on-Regular-command-lists)

# Overview

The implementation must support the ZEX_counter_based_event extension.   
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
- IPC sharing is one-directional. IPC CB Event opened in different process can be used only for waiting. If original Event state is changed (for example by next append call) and second process needs to see that update, IPC handle must be opened again.
- Regular command list (known as recorded or non-immediate) is a special use case for CB Events. Will be described in separate section

Regular Event rely on memory state controlled by the User (explicit Reset calls). CB Event represents host programming sequence, without managing the state. For example:
 
```cpp
zexCounterBasedEventCreate2(context, device, &desc, &event1); // counter not yet assigned

zeCommandListAppendLaunchKernel(cmdList1, kernel, &groupCount, &event1, 0, nullptr); // assigned counter=X on memory CL1_alloc
zeCommandListAppendLaunchKernel(cmdList2, kernel, &groupCount, nullptr, 1, &event1); // cmdList2 waits for counter=X on memory CL1_alloc

// reuse without waiting/reset
zeCommandListAppendLaunchKernel(cmdList3, kernel, &groupCount, &event1, 0, nullptr); // Replace state. Assigned counter=Y on memory CL3_alloc

// Event1 is implicitly reset to different state.
// cmdList2 can be still running on GPU. It waits for counter=X on memory CL1_alloc. 
// Its also safe to delete Event object.

zeEventHostSynchronize(event1); // wait for counter=Y on memory CL3_alloc
```

# Creation
There were multiple attempts to define creation API.  
**1 and 2 are deprecated**  
  
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
Existing `zeEventCreate` with Event Pool flags: `ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE` / `ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE`.  

### 3. 
```cpp
ze_result_t zexCounterBasedEventCreate2(
                ze_context_handle_t hContext, 
                ze_device_handle_t hDevice, 
                const zex_counter_based_event_desc_t *desc, 
                ze_event_handle_t *phEvent);
```

Third option is the most recent one. It combines both previous attempts. Including mapping of existing Event Pool flags (Timestamp, IPC, etc.) and external storage.  
`zex_counter_based_event_desc_t` structure defines all needed parameters. At least one of the `ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE` / `ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE` flags must be set.  

Long term goal is to use only third option and deprecate 1 and 2.

# External storage
User may optionally specify externally managed counter allocation and value. This can be done by passing `zex_counter_based_event_external_sync_alloc_properties_t` as extension of `zex_counter_based_event_desc_t`.  

**Requirements:**
- Counter allocation is managed by the User. May be allocated outside L0 Driver. Some limitations may be expected
- User must ensure device allocation (`deviceAddress`) residency. It must be accessible by GPU
- Host allocation (`hostAddress`) must be accessible by CPU (eg. waiting for completion)
- User is responsible for updating both memory locations to >= `completionValue` to signal Event completion
- Signaling such event, replaces the state (as described previously)

# Aggregated event
Aggregated event is a special use case for CB Events. It can be signaled from multiple append calls, but waiting requires only one memory compare operation.  
It can be created by passing `zex_counter_based_event_external_storage_properties_t` as extension of `zex_counter_based_event_desc_t`.

**Requirements:**
- This extension cannot be used with "external storage" extension
- User must ensure device allocation (`deviceAddress`) residency. It must be accessible by GPU
- Driver will use `deviceAddress` for host synchronization as USM allocation. It must be accessible by CPU
- Signaling such event, will not replace its state (as described previously). It can be passed to multiple append calls and each append will increment the storage by `incrementValue` (atomically) on GPU
- Using aggregated event as dependency, requires only one memory compare operation against final value: `completionValue` >=  `*deviceAddress`
- Device storage is under Users control. It may be reset manually if needed
- Profiling is not possible if producers originate on different GPUs (different timestamp domains)

# Obtaining counter memory and value
User may obtain counter memory location and value. For example, waiting for completion outside the L0 Driver.  
If Event state is replaced by new append call or `zeCommandQueueExecuteCommandLists` that signals such Event, below API must be called again to obtain new data.

```cpp
ze_result_t zexEventGetDeviceAddress(
                ze_event_handle_t event,
                uint64_t *completionValue,
                uint64_t *address);
```

# IPC sharing
As mentioned previously, signaling CB Event replaces its state. This is why IPC sharing is one-directional. Opened event can be used only for waiting/querying (on host and GPU).

Both Event object (original and shared) are independent. There is no need to wait for completion before reusing.  
Second process points to the original state until `zeCounterBasedEventCloseIpcHandle` is called.  
Original Event state may be changed without waiting for completion. Second process is not affected.  

**Timestamps are not allowed for IPC sharing.**

```cpp
// process 1
desc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_IPC;
zexCounterBasedEventCreate2(context, device, &desc, &event1);

zeCommandListAppendLaunchKernel(cmdList1, kernel, &groupCount, &event1, 0, nullptr); // assigned counter=X on memory CL1_alloc to event1
zexCounterBasedEventGetIpcHandle(event1, &ipcHandle); // handle obtained after append call

// process 2
zexCounterBasedEventOpenIpcHandle(context2, ipcHandle, &event2); // event2 points to the same counter=X on memory CL1_alloc

// process 1
zeCommandListAppendLaunchKernel(cmdList2, kernel, &groupCount, &event1, 0, nullptr); // assigned counter=Y on memory CL2_alloc to event1. Event2 is not affected

// process 2
// event2 still points to counter=X on memory CL1_alloc. Can be used for waiting.
zexCounterBasedEventCloseIpcHandle(event2); // Free if not needed
```

### IPC handle creation
To share a CB Event across processes, an IPC handle can be obtained as follows:
```cpp
ze_result_t zexCounterBasedEventGetIpcHandle(ze_event_handle_t hEvent, zex_ipc_counter_based_event_handle_t *phIpc);
```
* `hEvent`: Handle of the event object.
* `phIpc`: Pointer to the IPC handle to be populated.

### IPC handle opening
In a different process, the IPC handle can be opened to create a new event object that points to the same counter memory location/value:
```cpp
ze_result_t zexCounterBasedEventOpenIpcHandle(ze_context_handle_t hContext, zex_ipc_counter_based_event_handle_t hIpc, ze_event_handle_t *phEvent);
```

### IPC handle closing
Once the IPC handle is no longer needed, new Event can be closed:
```cpp
ze_result_t zexCounterBasedEventCloseIpcHandle(ze_event_handle_t hEvent);
```

# Regular command list
Regular command list is a special use case for CB Events. Its state is additionally updated on every `zeCommandQueueExecuteCommandLists` call.  
Any API call that relies on explicit counter memory/value (eg. `zexEventGetDeviceAddress`) needs to be called again to obtain new data. This includes IPC.  
Other API calls that don't specify counter explicitly, are managed by the Driver.  

**Each regular command list execution updates state of the events that will be signaled in that command list to "not ready".**  
**This rule applies to `zeCommandQueueExecuteCommandLists` and `zeCommandListImmediateAppendCommandListsExp` API calls.**

```cpp
// in-order operations
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, nullptr, 0, nullptr);
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, &event1, 0, nullptr); // signalEvent on 2nd operation
zeCommandListAppendLaunchKernel(regularCmdList1, kernel, &groupCount, nullptr, 0, nullptr);
zeCommandListClose(regularCmdList1);

zeCommandQueueExecuteCommandLists(cmdQueue, 1, &regularCmdList1, nullptr); // Command list has 3 operations. Counter updated to {1->2->3}
zeCommandQueueExecuteCommandLists(cmdQueue, 1, &regularCmdList1, nullptr); // Execute again. Counter updated to {4->5->6}

// wait from different command list
zeCommandListAppendLaunchKernel(regularCmdList2, kernel, &groupCount, nullptr, 1, &event1); // wait for counter=5 (second operation, after 2nd execute call)
zeCommandListClose(regularCmdList2);
zeCommandQueueExecuteCommandLists(cmdQueue, 1, &regularCmdList2, nullptr); // wait for counter=5

// execute regularCmdList1 3rd time
zeCommandQueueExecuteCommandLists(cmdQueue, 1, &regularCmdList1, nullptr); // Counter updated to {7->8->9}

// execute regularCmdList2 2nd time
zeCommandQueueExecuteCommandLists(cmdQueue, 1, &regularCmdList2, nullptr); // wait for counter=8
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
