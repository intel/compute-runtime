/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeEventPoolCreate_Tracing(ze_driver_handle_t hDriver,
                          const ze_event_pool_desc_t *desc,
                          uint32_t numDevices,
                          ze_device_handle_t *phDevices,
                          ze_event_pool_handle_t *phEventPool);

__zedllexport ze_result_t __zecall
zeEventPoolDestroy_Tracing(ze_event_pool_handle_t hEventPool);

__zedllexport ze_result_t __zecall
zeEventCreate_Tracing(ze_event_pool_handle_t hEventPool,
                      const ze_event_desc_t *desc,
                      ze_event_handle_t *phEvent);

__zedllexport ze_result_t __zecall
zeEventDestroy_Tracing(ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeEventPoolGetIpcHandle_Tracing(ze_event_pool_handle_t hEventPool,
                                ze_ipc_event_pool_handle_t *phIpc);

__zedllexport ze_result_t __zecall
zeEventPoolOpenIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_ipc_event_pool_handle_t hIpc,
                                 ze_event_pool_handle_t *phEventPool);

__zedllexport ze_result_t __zecall
zeEventPoolCloseIpcHandle_Tracing(ze_event_pool_handle_t hEventPool);

__zedllexport ze_result_t __zecall
zeCommandListAppendSignalEvent_Tracing(ze_command_list_handle_t hCommandList,
                                       ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendWaitOnEvents_Tracing(ze_command_list_handle_t hCommandList,
                                        uint32_t numEvents,
                                        ze_event_handle_t *phEvents);

__zedllexport ze_result_t __zecall
zeEventHostSignal_Tracing(ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeEventHostSynchronize_Tracing(ze_event_handle_t hEvent,
                               uint32_t timeout);

__zedllexport ze_result_t __zecall
zeEventQueryStatus_Tracing(ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeEventHostReset_Tracing(ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendEventReset_Tracing(ze_command_list_handle_t hCommandList,
                                      ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeEventGetTimestamp_Tracing(ze_event_handle_t hEvent,
                            ze_event_timestamp_type_t timestampType,
                            void *dstptr);
}
