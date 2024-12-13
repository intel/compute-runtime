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
    ze_device_handle_t device,                                   ///< [in] handle of the device
    const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc, ///< [in] pointer to external semaphore descriptor
    ze_intel_external_semaphore_exp_handle_t *phSemaphore        ///< [out] pointer to handle of the external semaphore
);

/// @brief Wait on External Semaphores
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelCommandListAppendWaitExternalSemaphoresExp(
    ze_command_list_handle_t hCmdList,                            ///< [in] handle of the command list
    unsigned int numExternalSemaphores,                           ///< [in] number of external semaphores
    const ze_intel_external_semaphore_exp_handle_t *phSemaphores, ///< [in] pointer to array of external semaphore handles
    const ze_intel_external_semaphore_wait_params_exp_t *params,  ///< [in] pointer to array of wait parameters
    ze_event_handle_t hSignalEvent,                               ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                                       ///< [in][optional] number of events to wait on before continuing
    ze_event_handle_t *phWaitEvents                               ///< [in][optional][range(0, numWaitEvents)] handles of the events to wait on before continuing
);

/// @brief Signal External Semaphores
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelCommandListAppendSignalExternalSemaphoresExp(
    ze_command_list_handle_t hCmdList,                             ///< [in] handle of the command list
    size_t numExternalSemaphores,                                  ///< [in] number of external semaphores
    const ze_intel_external_semaphore_exp_handle_t *phSemaphores,  ///< [in] pointer to array of external semaphore handles
    const ze_intel_external_semaphore_signal_params_exp_t *params, ///< [in] pointer to array of signal parameters
    ze_event_handle_t hSignalEvent,                                ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                                        ///< [in][optional] number of events to wait on before continuing
    ze_event_handle_t *phWaitEvents                                ///< [in][optional][range(0, numWaitEvents)] handles of the events to wait on before continuing
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

