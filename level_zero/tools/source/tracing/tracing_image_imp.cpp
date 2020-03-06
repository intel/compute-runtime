/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeImageGetProperties_Tracing(ze_device_handle_t hDevice,
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

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.ppImageProperties);
}

__zedllexport ze_result_t __zecall
zeImageCreate_Tracing(ze_device_handle_t hDevice,
                      const ze_image_desc_t *desc,
                      ze_image_handle_t *phImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Image.pfnCreate,
                               hDevice,
                               desc,
                               phImage);

    ze_image_create_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphImage = &phImage;

    L0::APITracerCallbackDataImp<ze_pfnImageCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnImageCreateCb_t, Image, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphImage);
}

__zedllexport ze_result_t __zecall
zeImageDestroy_Tracing(ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Image.pfnDestroy,
                               hImage);

    ze_image_destroy_params_t tracerParams;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnImageDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnImageDestroyCb_t, Image, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Image.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phImage);
}
