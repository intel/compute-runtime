/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCreateTracing(ze_context_handle_t hContext,
                         const ze_event_pool_desc_t *desc,
                         uint32_t numDevices,
                         ze_device_handle_t *phDevices,
                         ze_event_pool_handle_t *phEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnCreate,
                               hContext,
                               desc,
                               numDevices,
                               phDevices,
                               phEventPool);

    ze_event_pool_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pdesc = &desc;
    tracerParams.pnumDevices = &numDevices;
    tracerParams.pphDevices = &phDevices;
    tracerParams.pphEventPool = &phEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolCreateCb_t, EventPool, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pdesc,
                                   *tracerParams.pnumDevices,
                                   *tracerParams.pphDevices,
                                   *tracerParams.pphEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolDestroyTracing(ze_event_pool_handle_t hEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnDestroy,
                               hEventPool);

    ze_event_pool_destroy_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolDestroyCb_t, EventPool, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventCreateTracing(ze_event_pool_handle_t hEventPool,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventDestroyTracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnDestroy,
                               hEvent);

    ze_event_destroy_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventDestroyCb_t, Event, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolGetIpcHandleTracing(ze_event_pool_handle_t hEventPool,
                               ze_ipc_event_pool_handle_t *phIpc) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle,
                               hEventPool,
                               phIpc);

    ze_event_pool_get_ipc_handle_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;
    tracerParams.pphIpc = &phIpc;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolGetIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolGetIpcHandleCb_t, EventPool, pfnGetIpcHandleCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool,
                                   *tracerParams.pphIpc);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolOpenIpcHandleTracing(ze_context_handle_t hContext,
                                ze_ipc_event_pool_handle_t hIpc,
                                ze_event_pool_handle_t *phEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle,
                               hContext,
                               hIpc,
                               phEventPool);

    ze_event_pool_open_ipc_handle_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phIpc = &hIpc;
    tracerParams.pphEventPool = &phEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolOpenIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolOpenIpcHandleCb_t, EventPool, pfnOpenIpcHandleCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phIpc,
                                   *tracerParams.pphEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventPoolCloseIpcHandleTracing(ze_event_pool_handle_t hEventPool) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle,
                               hEventPool);

    ze_event_pool_close_ipc_handle_params_t tracerParams;
    tracerParams.phEventPool = &hEventPool;

    L0::APITracerCallbackDataImp<ze_pfnEventPoolCloseIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventPoolCloseIpcHandleCb_t, EventPool, pfnCloseIpcHandleCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEventPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendSignalEventTracing(ze_command_list_handle_t hCommandList,
                                      ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent,
                               hCommandList,
                               hEvent);

    ze_command_list_append_signal_event_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendSignalEventCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendSignalEventCb_t, CommandList, pfnAppendSignalEventCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWaitOnEventsTracing(ze_command_list_handle_t hCommandList,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pnumEvents,
                                   *tracerParams.pphEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSignalTracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostSignal,
                               hEvent);

    ze_event_host_signal_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventHostSignalCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostSignalCb_t, Event, pfnHostSignalCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostSignal,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostSynchronizeTracing(ze_event_handle_t hEvent,
                              uint64_t timeout) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize,
                               hEvent,
                               timeout);

    ze_event_host_synchronize_params_t tracerParams;
    tracerParams.phEvent = &hEvent;
    tracerParams.ptimeout = &timeout;

    L0::APITracerCallbackDataImp<ze_pfnEventHostSynchronizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostSynchronizeCb_t, Event, pfnHostSynchronizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent,
                                   *tracerParams.ptimeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryStatusTracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnQueryStatus,
                               hEvent);

    ze_event_query_status_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventQueryStatusCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventQueryStatusCb_t, Event, pfnQueryStatusCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnQueryStatus,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventHostResetTracing(ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnHostReset,
                               hEvent);

    ze_event_host_reset_params_t tracerParams;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnEventHostResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventHostResetCb_t, Event, pfnHostResetCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnHostReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendEventResetTracing(ze_command_list_handle_t hCommandList,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryKernelTimestampTracing(ze_event_handle_t hEvent,
                                   ze_kernel_timestamp_result_t *dstptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Event.pfnQueryKernelTimestamp,
                               hEvent,
                               dstptr);

    ze_event_query_kernel_timestamp_params_t tracerParams;
    tracerParams.phEvent = &hEvent;
    tracerParams.pdstptr = &dstptr;

    L0::APITracerCallbackDataImp<ze_pfnEventQueryKernelTimestampCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnEventQueryKernelTimestampCb_t, Event, pfnQueryKernelTimestampCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Event.pfnQueryKernelTimestamp,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phEvent,
                                   *tracerParams.pdstptr);
}
