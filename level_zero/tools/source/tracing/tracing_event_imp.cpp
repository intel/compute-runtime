/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeEventPoolCreate_Tracing(ze_driver_handle_t hDriver,
                          const ze_event_pool_desc_t *desc,
                          uint32_t numDevices,
                          ze_device_handle_t *phDevices,
                          ze_event_pool_handle_t *phEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnCreate,
                               hDriver,
                               desc,
                               numDevices,
                               phDevices,
                               phEventPool);

    ze_event_pool_create_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pdesc = &desc;
    tracerParams.pnumDevices = &numDevices;
    tracerParams.pphDevices = &phDevices;
    tracerParams.pphEventPool = &phEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolCreateCb_t, EventPool, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pdesc,
                                   *tracerParams.pnumDevices,
                                   *tracerParams.pphDevices,
                                   *tracerParams.pphEventPool);
}

__zedllexport ze_result_t __zecall
zeEventPoolDestroy_Tracing(ze_event_pool_handle_t hEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnDestroy,
                               hEventPool);

    ze_event_pool_destroy_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolDestroyCb_t, EventPool, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool);
}

__zedllexport ze_result_t __zecall
zeEventCreate_Tracing(ze_event_pool_handle_t hEventPool,
                      const ze_event_desc_t *desc,
                      ze_event_handle_t *phEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnCreate,
                               hEventPool,
                               desc,
                               phEvent);

    ze_event_create_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;
    tracerParams.pdesc = &desc;
    tracerParams.pphEvent = &phEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventCreateCb_t, Event, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphEvent);
}

__zedllexport ze_result_t __zecall
zeEventDestroy_Tracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnDestroy,
                               hEvent);

    ze_event_destroy_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventDestroyCb_t, Event, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeEventPoolGetIpcHandle_Tracing(ze_event_pool_handle_t hEventPool,
                                ze_ipc_event_pool_handle_t *phIpc) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle,
                               hEventPool,
                               phIpc);

    ze_event_pool_get_ipc_handle_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;
    tracerParams.pphIpc = &phIpc;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolGetIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolGetIpcHandleCb_t, EventPool, pfnGetIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool,
                                   *tracerParams.pphIpc);
}

__zedllexport ze_result_t __zecall
zeEventPoolOpenIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_ipc_event_pool_handle_t hIpc,
                                 ze_event_pool_handle_t *phEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle,
                               hDriver,
                               hIpc,
                               phEventPool);

    ze_event_pool_open_ipc_handle_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.phIpc = &hIpc;
    tracerParams.pphEventPool = &phEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolOpenIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolOpenIpcHandleCb_t, EventPool, pfnOpenIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.phIpc,
                                   *tracerParams.pphEventPool);
}

__zedllexport ze_result_t __zecall
zeEventPoolCloseIpcHandle_Tracing(ze_event_pool_handle_t hEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle,
                               hEventPool);

    ze_event_pool_close_ipc_handle_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolCloseIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolCloseIpcHandleCb_t, EventPool, pfnCloseIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendSignalEvent_Tracing(ze_command_list_handle_t hCommandList,
                                       ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent,
                               hCommandList,
                               hEvent);

    ze_command_list_append_signal_event_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendSignalEventCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendSignalEventCb_t, CommandList, pfnAppendSignalEventCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendWaitOnEvents_Tracing(ze_command_list_handle_t hCommandList,
                                        uint32_t numEvents,
                                        ze_event_handle_t *phEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents,
                               hCommandList,
                               numEvents,
                               phEvents);

    ze_command_list_append_wait_on_events_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pnumEvents = &numEvents;
    tracerParams.pphEvents = &phEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendWaitOnEventsCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendWaitOnEventsCb_t, CommandList, pfnAppendWaitOnEventsCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pnumEvents,
                                   *tracerParams.pphEvents);
}

__zedllexport ze_result_t __zecall
zeEventHostSignal_Tracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostSignal,
                               hEvent);

    ze_event_host_signal_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventHostSignalCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostSignalCb_t, Event, pfnHostSignalCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostSignal,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeEventHostSynchronize_Tracing(ze_event_handle_t hEvent,
                               uint32_t timeout) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize,
                               hEvent,
                               timeout);

    ze_event_host_synchronize_params_t tracerParams;
    tracerParams.phEvent = &hEvent;
    tracerParams.ptimeout = &timeout;

    L0::APITracerCallbackDataImp<ze_pfnEventHostSynchronizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostSynchronizeCb_t, Event, pfnHostSynchronizeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent,
                                   *tracerParams.ptimeout);
}

__zedllexport ze_result_t __zecall
zeEventQueryStatus_Tracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnQueryStatus,
                               hEvent);

    ze_event_query_status_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventQueryStatusCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventQueryStatusCb_t, Event, pfnQueryStatusCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnQueryStatus,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeEventHostReset_Tracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostReset,
                               hEvent);

    ze_event_host_reset_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventHostResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostResetCb_t, Event, pfnHostResetCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendEventReset_Tracing(ze_command_list_handle_t hCommandList,
                                      ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset,
                               hCommandList,
                               hEvent);

    ze_command_list_append_event_reset_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendEventResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendEventResetCb_t,
                                  CommandList, pfnAppendEventResetCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeEventGetTimestamp_Tracing(ze_event_handle_t hEvent,
                            ze_event_timestamp_type_t timestampType,
                            void *dstptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnGetTimestamp,
                               hEvent,
                               timestampType,
                               dstptr);

    ze_event_get_timestamp_params_t tracerParams;
    tracerParams.phEvent = &hEvent;
    tracerParams.ptimestampType = &timestampType;
    tracerParams.pdstptr = &dstptr;

    L0::APITracerCallbackDataImp<ze_pfnEventGetTimestampCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventGetTimestampCb_t, Event, pfnGetTimestampCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnGetTimestamp,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent,
                                   *tracerParams.ptimestampType,
                                   *tracerParams.pdstptr);
}
