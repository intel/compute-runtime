/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueCreateTracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            const ze_command_queue_desc_t *desc,
                            ze_command_queue_handle_t *phCommandQueue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandQueue.pfnCreate,
                               hContext,
                               hDevice,
                               desc,
                               phCommandQueue);

    ze_command_queue_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphCommandQueue = &phCommandQueue;

    L0::APITracerCallbackDataImp<ze_pfnCommandQueueCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandQueueCreateCb_t, CommandQueue, pfnCreateCb);
    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandQueue.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphCommandQueue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueDestroyTracing(ze_command_queue_handle_t hCommandQueue) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandQueue.pfnDestroy,
                               hCommandQueue);

    ze_command_queue_destroy_params_t tracerParams;
    tracerParams.phCommandQueue = &hCommandQueue;

    L0::APITracerCallbackDataImp<ze_pfnCommandQueueDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandQueueDestroyCb_t, CommandQueue, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandQueue.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandQueue);
}
ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueExecuteCommandListsTracing(ze_command_queue_handle_t hCommandQueue,
                                         uint32_t numCommandLists,
                                         ze_command_list_handle_t *phCommandLists,
                                         ze_fence_handle_t hFence) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandQueue.pfnExecuteCommandLists,
                               hCommandQueue,
                               numCommandLists,
                               phCommandLists,
                               hFence);

    ze_command_queue_execute_command_lists_params_t tracerParams;
    tracerParams.phCommandQueue = &hCommandQueue;
    tracerParams.pnumCommandLists = &numCommandLists;
    tracerParams.pphCommandLists = &phCommandLists;
    tracerParams.phFence = &hFence;
    L0::APITracerCallbackDataImp<ze_pfnCommandQueueExecuteCommandListsCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandQueueExecuteCommandListsCb_t, CommandQueue, pfnExecuteCommandListsCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandQueue.pfnExecuteCommandLists,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandQueue,
                                   *tracerParams.pnumCommandLists,
                                   *tracerParams.pphCommandLists,
                                   *tracerParams.phFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueSynchronizeTracing(ze_command_queue_handle_t hCommandQueue,
                                 uint64_t timeout) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandQueue.pfnSynchronize,
                               hCommandQueue,
                               timeout);

    ze_command_queue_synchronize_params_t tracerParams;
    tracerParams.phCommandQueue = &hCommandQueue;
    tracerParams.ptimeout = &timeout;

    L0::APITracerCallbackDataImp<ze_pfnCommandQueueSynchronizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandQueueSynchronizeCb_t, CommandQueue, pfnSynchronizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.CommandQueue.pfnSynchronize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandQueue,
                                   *tracerParams.ptimeout);
}
