/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeCommandListAppendBarrier_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phSignalEvent,
                                   *tracerParams.pnumWaitEvents,
                                   *tracerParams.pphWaitEvents);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryRangesBarrier_Tracing(ze_command_list_handle_t hCommandList,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier,
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

__zedllexport ze_result_t __zecall
zeDeviceSystemBarrier_Tracing(ze_device_handle_t hDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnSystemBarrier,
                               hDevice);

    ze_device_system_barrier_params_t tracerParams;
    tracerParams.phDevice = &hDevice;

    L0::APITracerCallbackDataImp<ze_pfnDeviceSystemBarrierCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceSystemBarrierCb_t, Device, pfnSystemBarrierCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnSystemBarrier,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice);
}
