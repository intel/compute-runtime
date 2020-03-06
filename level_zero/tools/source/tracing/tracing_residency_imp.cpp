/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeDeviceMakeMemoryResident_Tracing(ze_device_handle_t hDevice,
                                   void *ptr,
                                   size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnMakeMemoryResident,
                               hDevice,
                               ptr,
                               size);

    ze_device_make_memory_resident_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnDeviceMakeMemoryResidentCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceMakeMemoryResidentCb_t, Device, pfnMakeMemoryResidentCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnMakeMemoryResident,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

__zedllexport ze_result_t __zecall
zeDeviceEvictMemory_Tracing(ze_device_handle_t hDevice,
                            void *ptr,
                            size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnEvictMemory,
                               hDevice,
                               ptr,
                               size);

    ze_device_evict_memory_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnDeviceEvictMemoryCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceEvictMemoryCb_t, Device, pfnEvictMemoryCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnEvictMemory,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

__zedllexport ze_result_t __zecall
zeDeviceMakeImageResident_Tracing(ze_device_handle_t hDevice,
                                  ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnMakeImageResident, hDevice, hImage);

    ze_device_make_image_resident_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnDeviceMakeImageResidentCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceMakeImageResidentCb_t, Device, pfnMakeImageResidentCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnMakeImageResident,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}

__zedllexport ze_result_t __zecall
zeDeviceEvictImage_Tracing(ze_device_handle_t hDevice,
                           ze_image_handle_t hImage) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnEvictImage,
                               hDevice,
                               hImage);

    ze_device_evict_image_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.phImage = &hImage;

    L0::APITracerCallbackDataImp<ze_pfnDeviceEvictImageCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceEvictImageCb_t, Device, pfnEvictImageCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnEvictImage,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phImage);
}
