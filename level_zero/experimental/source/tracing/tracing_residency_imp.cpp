/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeMemoryResident_Tracing(ze_context_handle_t hContext,
                                    ze_device_handle_t hDevice,
                                    void *ptr,
                                    size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident,
                               hContext,
                               hDevice,
                               ptr,
                               size);

    ze_context_make_memory_resident_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnContextMakeMemoryResidentCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextMakeMemoryResidentCb_t, Context, pfnMakeMemoryResidentCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictMemory_Tracing(ze_context_handle_t hContext,
                             ze_device_handle_t hDevice,
                             void *ptr,
                             size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnEvictMemory,
                               hContext,
                               hDevice,
                               ptr,
                               size);

    ze_context_evict_memory_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnContextEvictMemoryCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextEvictMemoryCb_t, Context, pfnEvictMemoryCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnEvictMemory,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeImageResident_Tracing(ze_context_handle_t hContext,
                                   ze_device_handle_t hDevice,
                                   ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident,
                               hContext,
                               hDevice,
                               hImage);

    ze_context_make_image_resident_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnContextMakeImageResidentCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextMakeImageResidentCb_t, Context, pfnMakeImageResidentCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictImage_Tracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnEvictImage,
                               hContext,
                               hDevice,
                               hImage);

    ze_context_evict_image_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnContextEvictImageCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextEvictImageCb_t, Context, pfnEvictImageCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnEvictImage,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}
