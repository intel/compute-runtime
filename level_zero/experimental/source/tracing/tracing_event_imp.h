/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCreate_Tracing(ze_driver_handle_t hDriver,
                          const ze_event_pool_desc_t *desc,
                          uint32_t numDevices,
                          ze_device_handle_t *phDevices,
                          ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolDestroy_Tracing(ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventCreate_Tracing(ze_event_pool_handle_t hEventPool,
                      const ze_event_desc_t *desc,
                      ze_event_handle_t *phEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventDestroy_Tracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolGetIpcHandle_Tracing(ze_event_pool_handle_t hEventPool,
                                ze_ipc_event_pool_handle_t *phIpc);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolOpenIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_ipc_event_pool_handle_t hIpc,
                                 ze_event_pool_handle_t *phEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCloseIpcHandle_Tracing(ze_event_pool_handle_t hEventPool);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendSignalEvent_Tracing(ze_command_list_handle_t hCommandList,
                                       ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWaitOnEvents_Tracing(ze_command_list_handle_t hCommandList,
                                        uint32_t numEvents,
                                        ze_event_handle_t *phEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSignal_Tracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSynchronize_Tracing(ze_event_handle_t hEvent,
                               uint32_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryStatus_Tracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostReset_Tracing(ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendEventReset_Tracing(ze_command_list_handle_t hCommandList,
                                      ze_event_handle_t hEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventGetTimestamp_Tracing(ze_event_handle_t hEvent,
                            ze_event_timestamp_type_t timestampType,
                            void *dstptr);
}
