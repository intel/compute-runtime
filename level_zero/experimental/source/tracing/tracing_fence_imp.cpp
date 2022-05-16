/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceCreateTracing(ze_command_queue_handle_t hCommandQueue,
                     const ze_fence_desc_t *desc,
                     ze_fence_handle_t *phFence) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Fence.pfnCreate,
                               hCommandQueue,
                               desc,
                               phFence);

    ze_fence_create_params_t tracerParams;
    tracerParams.phCommandQueue = &hCommandQueue;
    tracerParams.pdesc = &desc;
    tracerParams.pphFence = &phFence;

    L0::APITracerCallbackDataImp<ze_pfnFenceCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnFenceCreateCb_t, Fence, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Fence.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandQueue,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceDestroyTracing(ze_fence_handle_t hFence) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Fence.pfnDestroy,
                               hFence);

    ze_fence_destroy_params_t tracerParams;
    tracerParams.phFence = &hFence;

    L0::APITracerCallbackDataImp<ze_pfnFenceDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnFenceDestroyCb_t, Fence, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Fence.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceHostSynchronizeTracing(ze_fence_handle_t hFence,
                              uint64_t timeout) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize,
                               hFence,
                               timeout);

    ze_fence_host_synchronize_params_t tracerParams;
    tracerParams.phFence = &hFence;
    tracerParams.ptimeout = &timeout;

    L0::APITracerCallbackDataImp<ze_pfnFenceHostSynchronizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnFenceHostSynchronizeCb_t, Fence, pfnHostSynchronizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phFence,
                                   *tracerParams.ptimeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceQueryStatusTracing(ze_fence_handle_t hFence) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus,
                               hFence);

    ze_fence_query_status_params_t tracerParams;
    tracerParams.phFence = &hFence;

    L0::APITracerCallbackDataImp<ze_pfnFenceQueryStatusCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnFenceQueryStatusCb_t, Fence, pfnQueryStatusCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceResetTracing(ze_fence_handle_t hFence) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Fence.pfnReset,
                               hFence);

    ze_fence_reset_params_t tracerParams;
    tracerParams.phFence = &hFence;

    L0::APITracerCallbackDataImp<ze_pfnFenceResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnFenceResetCb_t, Fence, pfnResetCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Fence.pfnReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phFence);
}
