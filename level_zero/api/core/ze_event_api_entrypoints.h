/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeEventPoolCreate(
    ze_context_handle_t hContext,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool);

ze_result_t ZE_APICALL zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool);

ze_result_t ZE_APICALL zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zeEventDestroy(
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc);

ze_result_t ZE_APICALL zeEventPoolOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool);

ze_result_t ZE_APICALL zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool);

ze_result_t ZE_APICALL zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents);

ze_result_t ZE_APICALL zeEventHostSignal(
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint64_t timeout);

ze_result_t ZE_APICALL zeEventQueryStatus(
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeEventHostReset(
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeEventQueryKernelTimestamp(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *timestampType);

ze_result_t ZE_APICALL zeEventQueryKernelTimestampsExt(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_event_query_kernel_timestamps_results_ext_properties_t *pResults);

ze_result_t ZE_APICALL zeEventPoolPutIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc);

ze_result_t ZE_APICALL zeEventPoolGetContextHandle(
    ze_event_pool_handle_t hEventPool,
    ze_context_handle_t *phContext);

ze_result_t ZE_APICALL zeEventPoolGetFlags(
    ze_event_pool_handle_t hEventPool,
    ze_event_pool_flags_t *pFlags);

ze_result_t ZE_APICALL zeEventGetEventPool(
    ze_event_handle_t hEvent,
    ze_event_pool_handle_t *phEventPool);

ze_result_t ZE_APICALL zeEventGetSignalScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pSignalScope);

ze_result_t ZE_APICALL zeEventGetWaitScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pWaitScope);

ze_result_t ZE_APICALL zeEventCounterBasedCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_event_counter_based_desc_t *desc,
    ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zeEventCounterBasedGetDeviceAddress(
    ze_event_handle_t hEvent,
    uint64_t *completionValue,
    uint64_t *deviceAddress);

ze_result_t ZE_APICALL zeEventCounterBasedGetIpcHandle(
    ze_event_handle_t hEvent,
    ze_ipc_event_counter_based_handle_t *phIpc);

ze_result_t ZE_APICALL zeEventCounterBasedOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_counter_based_handle_t hIpc,
    ze_event_handle_t *phEvent);

ze_result_t ZE_APICALL zeEventCounterBasedCloseIpcHandle(
    ze_event_handle_t hEvent);

ze_result_t ZE_APICALL zeEventGetCounterBasedFlags(
    ze_event_handle_t hEvent,
    ze_event_counter_based_flags_t *pFlags);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolCreate(
    ze_context_handle_t hContext,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventDestroy(
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostSignal(
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryStatus(
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventHostReset(
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryKernelTimestamp(
    ze_event_handle_t hEvent,
    ze_kernel_timestamp_result_t *dstptr);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventQueryKernelTimestampsExt(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_event_query_kernel_timestamps_results_ext_properties_t *pResults);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetContextHandle(
    ze_event_pool_handle_t hEventPool,
    ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventPoolGetFlags(
    ze_event_pool_handle_t hEventPool,
    ze_event_pool_flags_t *pFlags);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetEventPool(
    ze_event_handle_t hEvent,
    ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetSignalScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pSignalScope);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetWaitScope(
    ze_event_handle_t hEvent,
    ze_event_scope_flags_t *pWaitScope);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCounterBasedCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_event_counter_based_desc_t *desc,
    ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCounterBasedGetDeviceAddress(
    ze_event_handle_t hEvent,
    uint64_t *completionValue,
    uint64_t *deviceAddress);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCounterBasedGetIpcHandle(
    ze_event_handle_t hEvent,
    ze_ipc_event_counter_based_handle_t *phIpc);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCounterBasedOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_event_counter_based_handle_t hIpc,
    ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventCounterBasedCloseIpcHandle(
    ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL zeEventGetCounterBasedFlags(
    ze_event_handle_t hEvent,
    ze_event_counter_based_flags_t *pFlags);

} // extern "C"
