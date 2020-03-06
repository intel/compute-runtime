/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeInit_Tracing(ze_init_flag_t flags) {
    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Global.pfnInit,
                               flags);

    ze_init_params_t tracerParams;
    tracerParams.pflags = &flags;

    L0::APITracerCallbackDataImp<ze_pfnInitCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnInitCb_t, Global, pfnInitCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Global.pfnInit,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.pflags);
}
