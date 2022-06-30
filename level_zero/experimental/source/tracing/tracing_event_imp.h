/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCreateTracing(ze_context_handle_t hContext,
                         const ze_event_pool_desc_t *desc,
                         uint32_t numDevices,
                         ze_device_handle_t *phDevices,
                         ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolDestroyTracing(ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventCreateTracing(ze_event_pool_handle_t hEventPool,
                     const ze_event_desc_t *desc,
                     ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventDestroyTracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolGetIpcHandleTracing(ze_event_pool_handle_t hEventPool,
                               ze_ipc_event_pool_handle_t *phIpc);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolOpenIpcHandleTracing(ze_context_handle_t hContext,
                                ze_ipc_event_pool_handle_t hIpc,
                                ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCloseIpcHandleTracing(ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendSignalEventTracing(ze_command_list_handle_t hCommandList,
                                      ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWaitOnEventsTracing(ze_command_list_handle_t hCommandList,
                                       uint32_t numEvents,
                                       ze_event_handle_t *phEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSignalTracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSynchronizeTracing(ze_event_handle_t hEvent,
                              uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryStatusTracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostResetTracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendEventResetTracing(ze_command_list_handle_t hCommandList,
                                     ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryKernelTimestampTracing(ze_event_handle_t hEvent,
                                   ze_kernel_timestamp_result_t *dstptr);
}
