/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerCreateTracing(ze_context_handle_t hContext,
                       ze_device_handle_t hDevice,
                       const ze_sampler_desc_t *pDesc,
                       ze_sampler_handle_t *phSampler) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Sampler.pfnCreate,
                               hContext,
                               hDevice,
                               pDesc,
                               phSampler);

    ze_sampler_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &pDesc;
    tracerParams.pphSampler = &phSampler;

    L0::APITracerCallbackDataImp<ze_pfnSamplerCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnSamplerCreateCb_t, Sampler, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Sampler.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphSampler);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeSamplerDestroyTracing(ze_sampler_handle_t hSampler) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Sampler.pfnDestroy,
                               hSampler);

    ze_sampler_destroy_params_t tracerParams;
    tracerParams.phSampler = &hSampler;

    L0::APITracerCallbackDataImp<ze_pfnSamplerDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnSamplerDestroyCb_t, Sampler, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Sampler.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phSampler);
}
