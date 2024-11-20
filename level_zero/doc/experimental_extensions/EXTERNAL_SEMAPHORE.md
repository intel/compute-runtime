<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# External Semaphore Extension

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

Users need a solution that allows for a fully asynchronous rendering pipeline in interop applications, as the existing solutions only allow for synchronization in the host level in these scenarios. This extension provides an API that will take the external semaphores from other graphics APIs and use it to synchronize to support the interoperability.

# Definitions

```cpp
#define ZE_INTEL_EXTERNAL_SEMAPHORE_EXP_NAME "ZE_intel_experimental_external_semaphore"

typedef enum _ze_intel_external_semaphore_exp_version_t {
    ZE_EXTERNAL_SEMAPHORE_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_EXTERNAL_SEMAPHORE_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZE_EXTERNAL_SEMAPHORE_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} ze_intel_external_semaphore_exp_version_t;

typedef enum _ze_intel_external_semaphore_exp_flags_t {
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_FD,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32_KMT,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_D3D12_FENCE,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_D3D11_FENCE,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_KEYED_MUTEX,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_KEYED_MUTEX_KMT,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_TIMELINE_SEMAPHORE_FD,
    ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_TIMELINE_SEMAPHORE_WIN32
} ze_intel_external_semaphore_exp_flags_t;

typedef struct _ze_intel_external_semaphore_exp_desc_t {
    ze_structure_type_t stype;
    const void *pNext;
    ze_intel_external_semaphore_exp_flags_t flags;
} ze_intel_external_semaphore_exp_desc_t;

typedef struct _ze_intel_external_semaphore_win32_exp_desc_t {
    ze_structure_type_t stype;
    const void *pNext;
    const char *name;
} ze_intel_external_semaphore_win32_exp_desc_t;

typedef struct _ze_intel_external_semaphore_fd_exp_desc_t {
    ze_structure_type_t stype;
    const void *pNext;
    int fd;
} ze_intel_external_semaphore_desc_fd_exp_desc_t;

typedef struct _ze_intel_external_semaphore_signal_exp_params_t {
    ze_structure_type_t stype;
    const void *pNext;
    uint64_t value;
} ze_intel_external_semaphore_signal_exp_params_t;

typedef struct _ze_intel_external_semaphore_wait_exp_params_t {
    ze_structure_type_t stype;
    const void *pNext;

    uint64_t value;
} ze_intel_external_semaphore_wait_exp_params_t;
```

## Interfaces

```cpp
/// @brief Import External Semaphore
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelDeviceImportExternalSemaphoreExp(
    ze_device_handle_t device, ///< [in] device handle
    ze_intel_external_semaphore_exp_handle_t *phSemaphore, ///< [out] semaphore handle
    const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc ///< [in] description of semaphore to be imported
);

/// @brief Wait on External Semaphores
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelCommandListAppendWaitExternalSemaphoresExp(
    ze_command_list_handle_t hCmdList, ///< [in] command list handle to append semaphore
    const ze_intel_external_semaphore_exp_handle_t *hSemaphores, ///< [in] array of semaphores to be appended into command list
    const ze_intel_external_semaphore_wait_exp_params_t *params, ///< [in] wait parameters for each element in array of semaphores 
    unsigned int numExternalSemaphores, ///< [in] total number of semaphores 
    ze_event_handle_t hSignalEvent, ///< [in] event to signal after waiting on the semaphores 
    uint32_t numWaitEvents, ///< [in] total number of events to wait before waiting on the semaphores
    ze_event_handle_t *phWaitEvents ///< [in] array of events to wait before waiting on the semaphores
);

/// @brief Signal External Semaphores
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelCommandListAppendSignalExternalSemaphoresExp(
    ze_command_list_handle_t hCmdList, ///< [in] command list handle to append semaphore
    const ze_intel_external_semaphore_exp_handle_t *hSemaphores, ///< [in] array of semaphores to be appended into command list
    const ze_intel_external_semaphore_signal_exp_params_t *params, ///< [in] signal parameters for each element in array of semaphores 
    unsigned int numExternalSemaphores, ///< [in] total number of semaphores 
    ze_event_handle_t hSignalEvent, ///< [in] event to signal after signaling the semaphores 
    uint32_t numWaitEvents, ///< [in] total number of events to wait before signaling the semaphores
    ze_event_handle_t *phWaitEvents ///< [in] array of events to wait before signaling the semaphores
);

/// @brief Release External Semaphore
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelDeviceReleaseExternalSemaphoreExp(
    ze_intel_external_semaphore_exp_handle_t *hSemaphore ///< [in] semaphore that is no longer used thus released by level-zero
);
```

## Programming example

# Known Issues and Limitations

