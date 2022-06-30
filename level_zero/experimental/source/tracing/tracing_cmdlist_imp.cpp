/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateTracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           const ze_command_list_desc_t *desc,
                           ze_command_list_handle_t *phCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnCreate,
                               hContext,
                               hDevice,
                               desc,
                               phCommandList);

    ze_command_list_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphCommandList = &phCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCreateCb_t, CommandList, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediateTracing(ze_context_handle_t hContext,
                                    ze_device_handle_t hDevice,
                                    const ze_command_queue_desc_t *altdesc,
                                    ze_command_list_handle_t *phCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnCreateImmediate,
                               hContext,
                               hDevice,
                               altdesc,
                               phCommandList);

    ze_command_list_create_immediate_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.paltdesc = &altdesc;
    tracerParams.pphCommandList = &phCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCreateImmediateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCreateImmediateCb_t, CommandList, pfnCreateImmediateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnCreateImmediate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.paltdesc,
                                   *tracerParams.pphCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListDestroyTracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnDestroy, hCommandList);

    ze_command_list_destroy_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListDestroyCb_t, CommandList, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCloseTracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnClose, hCommandList);

    ze_command_list_close_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCloseCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCloseCb_t, CommandList, pfnCloseCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnClose,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListResetTracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnReset,
                               hCommandList);

    ze_command_list_reset_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListResetCb_t, CommandList, pfnResetCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendWriteGlobalTimestampTracing(ze_command_list_handle_t hCommandList,
                                               uint64_t *dstptr,
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendWriteGlobalTimestamp,
                               hCommandList,
                               dstptr,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_write_global_timestamp_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pdstptr = &dstptr;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendWriteGlobalTimestampCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendWriteGlobalTimestampCb_t, CommandList, pfnAppendWriteGlobalTimestampCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendWriteGlobalTimestamp,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pdstptr,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendQueryKernelTimestampsTracing(ze_command_list_handle_t hCommandList,
                                                uint32_t numEvents,
                                                ze_event_handle_t *phEvents,
                                                void *dstptr,
                                                const size_t *pOffsets,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendQueryKernelTimestamps,
                               hCommandList,
                               numEvents,
                               phEvents,
                               dstptr,
                               pOffsets,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_query_kernel_timestamps_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pnumEvents = &numEvents;
    tracerParams.pphEvents = &phEvents;
    tracerParams.pdstptr = &dstptr;
    tracerParams.ppOffsets = &pOffsets;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendQueryKernelTimestampsCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendQueryKernelTimestampsCb_t, CommandList, pfnAppendQueryKernelTimestampsCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendQueryKernelTimestamps,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pnumEvents,
                                   *tracerParams.pphEvents,
                                   *tracerParams.pdstptr,
                                   *tracerParams.ppOffsets,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}
