/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendBarrierTracing(ze_command_list_handle_t hCommandList,
                                  ze_event_handle_t hSignalEvent,
                                  uint32_t numWaitEvents,
                                  ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier,
                               hCommandList,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_barrier_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendBarrierCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendBarrierCb_t, CommandList, pfnAppendBarrierCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryRangesBarrierTracing(ze_command_list_handle_t hCommandList,
                                              uint32_t numRanges,
                                              const size_t *pRangeSizes,
                                              const void **pRanges,
                                              ze_event_handle_t hSignalEvent,
                                              uint32_t numWaitEvents,
                                              ze_event_handle_t *phWaitEvents) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier,
                               hCommandList,
                               numRanges,
                               pRangeSizes,
                               pRanges,
                               hSignalEvent,
                               numWaitEvents,
                               phWaitEvents);

    ze_command_list_append_memory_ranges_barrier_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pnumRanges = &numRanges;
    tracerParams.ppRangeSizes = &pRangeSizes;
    tracerParams.ppRanges = &pRanges;
    tracerParams.phSignalEvent = &hSignalEvent;
    tracerParams.pnumWaitEvents = &numWaitEvents;
    tracerParams.pphWaitEvents = &phWaitEvents;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemoryRangesBarrierCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListAppendMemoryRangesBarrierCb_t, CommandList, pfnAppendMemoryRangesBarrierCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pnumRanges,
                                   *tracerParams.ppRangeSizes,
                                   *tracerParams.ppRanges,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}
