/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreate_Tracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            const ze_command_list_desc_t *desc,
                            ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediate_Tracing(ze_context_handle_t hContext,
                                     ze_device_handle_t hDevice,
                                     const ze_command_queue_desc_t *altdesc,
                                     ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListDestroy_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListClose_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListReset_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWriteGlobalTimestamp_Tracing(ze_command_list_handle_t hCommandList,
                                                uint64_t *dstptr,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents);
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendQueryKernelTimestamps_Tracing(ze_command_list_handle_t hCommandList,
                                                 uint32_t numEvents,
                                                 ze_event_handle_t *phEvents,
                                                 void *dstptr,
                                                 const size_t *pOffsets,
                                                 ze_event_handle_t hSignalEvent,
                                                 uint32_t numWaitEvents,
                                                 ze_event_handle_t *phWaitEvents);
}
