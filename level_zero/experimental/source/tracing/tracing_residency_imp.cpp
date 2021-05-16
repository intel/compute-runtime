/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreate_Tracing(ze_driver_handle_t hDriver,
                        const ze_context_desc_t *desc,
                        ze_context_handle_t *phContext) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnCreate,
                               hDriver,
                               desc,
                               phContext);

    ze_context_create_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pdesc = &desc;
    tracerParams.pphContext = &phContext;

    L0::APITracerCallbackDataImp<ze_pfnContextCreateCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextCreateCb_t, Context, pfnCreateCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnCreate,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroy_Tracing(ze_context_handle_t hContext) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnDestroy,
                               hContext);

    ze_context_destroy_params_t tracerParams;
    tracerParams.phContext = &hContext;

    L0::APITracerCallbackDataImp<ze_pfnContextDestroyCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextDestroyCb_t, Context, pfnDestroyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnDestroy,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatus_Tracing(ze_context_handle_t hContext) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnGetStatus,
                               hContext);

    ze_context_get_status_params_t tracerParams;
    tracerParams.phContext = &hContext;

    L0::APITracerCallbackDataImp<ze_pfnContextGetStatusCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextGetStatusCb_t, Context, pfnGetStatusCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnGetStatus,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextSystemBarrier_Tracing(ze_context_handle_t hContext,
                               ze_device_handle_t hDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier,
                               hContext,
                               hDevice);

    ze_context_system_barrier_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;

    L0::APITracerCallbackDataImp<ze_pfnContextSystemBarrierCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnContextSystemBarrierCb_t, Context, pfnSystemBarrierCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice);
}

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
