<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Event sync mode

* [Overview](#Overview)
* [Definitions](#Definitions)

# Overview

Driver must support `ZE_intel_experimental_event_sync_mode` extension.  
It allows user to optimize Event synchronization. 

`ze_intel_event_sync_mode_exp_desc_t` struct must be passed as `pNext` in `ze_event_desc_t` during Event creation.

## Low power wait

When this flag is enabled, driver will optimize `zeEventHostSynchronize` call to use CPU threads more efficiently. For example, instead of active polling on memory location, it may use OS methods to sleep CPU thread.  
Changing this mode may impact completion latency.

## Generating interrupt

When this flag is enabled, driver may program additional GPU commands related to signaling Event on the Device. Those commands will generate system interrupt.  
Interrupt may be used as additional signal to wake up CPU thread that is waiting for Event completion in low power mode.  
Driver may select which API calls are applicable for generating interrupts.

# Definitions

```cpp
typedef uint32_t ze_intel_event_sync_mode_exp_flags_t;
typedef enum _ze_intel_event_sync_mode_exp_flag_t {
    ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT = ZE_BIT(0),
    ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT = ZE_BIT(1),
    ZE_INTEL_EVENT_SYNC_MODE_EXP_EXP_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_intel_event_sync_mode_exp_flag_t;

#define ZE_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC (ze_structure_type_t)0x00030016

typedef struct _ze_intel_event_sync_mode_exp_desc_t {
    ze_structure_type_t stype;
    const void *pNext;

    ze_intel_event_sync_mode_exp_flags_t syncModeFlags;
} ze_intel_event_sync_mode_exp_desc_t;
```

## Programming example

```cpp
// Create Event pool
ze_event_pool_handle_t eventPoolHandle = createEventPool();

// Define extension settings
ze_intel_event_sync_mode_exp_desc_t eventSyncMode = {};
eventSyncMode.stype = ZE_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC;
eventSyncMode.syncModeFlags = ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT | ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT;

// Pass extension struct to Event descriptor
ze_event_desc_t eventDesc = createEventDesc();
eventDesc.pNext = &eventSyncMode;

// Create Event
ze_event_handle eventHandle;
zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle);
```
