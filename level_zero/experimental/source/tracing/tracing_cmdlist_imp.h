/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateTracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           const ze_command_list_desc_t *desc,
                           ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediateTracing(ze_context_handle_t hContext,
                                    ze_device_handle_t hDevice,
                                    const ze_command_queue_desc_t *altdesc,
                                    ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListDestroyTracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCloseTracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListResetTracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWriteGlobalTimestampTracing(ze_command_list_handle_t hCommandList,
                                               uint64_t *dstptr,
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents);
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendQueryKernelTimestampsTracing(ze_command_list_handle_t hCommandList,
                                                uint32_t numEvents,
                                                ze_event_handle_t *phEvents,
                                                void *dstptr,
                                                const size_t *pOffsets,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents);
}
