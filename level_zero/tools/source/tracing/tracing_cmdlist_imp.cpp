/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeCommandListCreate_Tracing(ze_device_handle_t hDevice,
                            const ze_command_list_desc_t *desc,
                            ze_command_list_handle_t *phCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnCreate,
                               hDevice,
                               desc,
                               phCommandList);

    ze_command_list_create_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphCommandList = &phCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCreateCb_t, CommandList, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListCreateImmediate_Tracing(
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnCreateImmediate,
                               hDevice,
                               altdesc,
                               phCommandList);

    ze_command_list_create_immediate_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.paltdesc = &altdesc;
    tracerParams.pphCommandList = &phCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCreateImmediateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCreateImmediateCb_t, CommandList, pfnCreateImmediateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnCreateImmediate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.paltdesc,
                                   *tracerParams.pphCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListDestroy_Tracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnDestroy, hCommandList);

    ze_command_list_destroy_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListDestroyCb_t, CommandList, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListClose_Tracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnClose, hCommandList);

    ze_command_list_close_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListCloseCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListCloseCb_t, CommandList, pfnCloseCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnClose,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListReset_Tracing(ze_command_list_handle_t hCommandList) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnReset,
                               hCommandList);

    ze_command_list_reset_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;

    L0::APITracerCallbackDataImp<ze_pfnCommandListResetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnCommandListResetCb_t, CommandList, pfnResetCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnReset,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phCommandList);
}
