/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextCreateTracing(ze_driver_handle_t hDriver,
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

    L0::APITracerCallbackDataImp<ze_pfnContextCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextCreateCb_t, Context, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextDestroyTracing(ze_context_handle_t hContext) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnDestroy,
                               hContext);

    ze_context_destroy_params_t tracerParams;
    tracerParams.phContext = &hContext;

    L0::APITracerCallbackDataImp<ze_pfnContextDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextDestroyCb_t, Context, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextGetStatusTracing(ze_context_handle_t hContext) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnGetStatus,
                               hContext);

    ze_context_get_status_params_t tracerParams;
    tracerParams.phContext = &hContext;

    L0::APITracerCallbackDataImp<ze_pfnContextGetStatusCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextGetStatusCb_t, Context, pfnGetStatusCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnGetStatus,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextSystemBarrierTracing(ze_context_handle_t hContext,
                              ze_device_handle_t hDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier,
                               hContext,
                               hDevice);

    ze_context_system_barrier_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;

    L0::APITracerCallbackDataImp<ze_pfnContextSystemBarrierCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextSystemBarrierCb_t, Context, pfnSystemBarrierCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeMemoryResidentTracing(ze_context_handle_t hContext,
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

    L0::APITracerCallbackDataImp<ze_pfnContextMakeMemoryResidentCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextMakeMemoryResidentCb_t, Context, pfnMakeMemoryResidentCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictMemoryTracing(ze_context_handle_t hContext,
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

    L0::APITracerCallbackDataImp<ze_pfnContextEvictMemoryCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextEvictMemoryCb_t, Context, pfnEvictMemoryCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnEvictMemory,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextMakeImageResidentTracing(ze_context_handle_t hContext,
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

    L0::APITracerCallbackDataImp<ze_pfnContextMakeImageResidentCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextMakeImageResidentCb_t, Context, pfnMakeImageResidentCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeContextEvictImageTracing(ze_context_handle_t hContext,
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

    L0::APITracerCallbackDataImp<ze_pfnContextEvictImageCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnContextEvictImageCb_t, Context, pfnEvictImageCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Context.pfnEvictImage,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}
