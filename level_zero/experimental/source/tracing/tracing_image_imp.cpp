/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetPropertiesTracing(ze_device_handle_t hDevice,
                            const ze_image_desc_t *desc,
                            ze_image_properties_t *pImageProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Image.pfnGetProperties,
                               hDevice,
                               desc,
                               pImageProperties);

    ze_image_get_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.ppImageProperties = &pImageProperties;

    L0::APITracerCallbackDataImp<ze_pfnImageGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnImageGetPropertiesCb_t, Image, pfnGetPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.ppImageProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageCreateTracing(ze_context_handle_t hContext,
                     ze_device_handle_t hDevice,
                     const ze_image_desc_t *desc,
                     ze_image_handle_t *phImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Image.pfnCreate,
                               hContext,
                               hDevice,
                               desc,
                               phImage);

    ze_image_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphImage = &phImage;

    L0::APITracerCallbackDataImp<ze_pfnImageCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnImageCreateCb_t, Image, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageDestroyTracing(ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Image.pfnDestroy,
                               hImage);

    ze_image_destroy_params_t tracerParams;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnImageDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnImageDestroyCb_t, Image, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phImage);
}
