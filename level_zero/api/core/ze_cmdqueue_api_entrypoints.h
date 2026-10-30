/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeCommandQueueCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *desc,
    ze_command_queue_handle_t *phCommandQueue);

ze_result_t ZE_APICALL zeCommandQueueDestroy(
    ze_command_queue_handle_t hCommandQueue);

ze_result_t ZE_APICALL zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence);

ze_result_t ZE_APICALL zeCommandQueueSynchronize(
    ze_command_queue_handle_t hCommandQueue,
    uint64_t timeout);

ze_result_t ZE_APICALL zeCommandQueueGetOrdinal(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pOrdinal);

ze_result_t ZE_APICALL zeCommandQueueGetIndex(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pIndex);

ze_result_t ZE_APICALL zeCommandQueueGetFlags(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_flags_t *pFlags);

ze_result_t ZE_APICALL zeCommandQueueGetMode(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_mode_t *pMode);

ze_result_t ZE_APICALL zeCommandQueueGetPriority(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_priority_t *pPriority);

ze_result_t ZE_APICALL zeCommandQueueSetPriorityExt(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_priority_t priority);

ze_result_t ZE_APICALL zeCommandQueueSetQosExt(
    ze_command_queue_handle_t hCommandQueue,
    const ze_command_queue_qos_ext_desc_t *desc);

ze_result_t ZE_APICALL zeCommandQueueGetQosExt(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_qos_ext_properties_t *pProperties);

ze_result_t ZE_APICALL zeCommandQueueSignalExternalSemaphoreExt(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandQueueWaitExternalSemaphoreExt(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *desc,
    ze_command_queue_handle_t *phCommandQueue);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueDestroy(
    ze_command_queue_handle_t hCommandQueue);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueSynchronize(
    ze_command_queue_handle_t hCommandQueue,
    uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueGetOrdinal(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pOrdinal);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueGetIndex(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t *pIndex);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueGetMode(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_mode_t *pMode);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueGetPriority(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_priority_t *pPriority);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueSetPriorityExt(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_priority_t priority);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueSetQosExt(
    ze_command_queue_handle_t hCommandQueue,
    const ze_command_queue_qos_ext_desc_t *desc);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueGetQosExt(
    ze_command_queue_handle_t hCommandQueue,
    ze_command_queue_qos_ext_properties_t *pProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueSignalExternalSemaphoreExt(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_signal_params_ext_t *signalParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandQueueWaitExternalSemaphoreExt(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numSemaphores,
    ze_external_semaphore_ext_handle_t *phSemaphores,
    ze_external_semaphore_wait_params_ext_t *waitParams,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

} // extern "C"
