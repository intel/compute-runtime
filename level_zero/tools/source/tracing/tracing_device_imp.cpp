/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeDeviceGet_Tracing(ze_driver_handle_t hDriver,
                    uint32_t *pCount,
                    ze_device_handle_t *phDevices) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGet,
                               hDriver,
                               pCount,
                               phDevices);

    ze_device_get_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.ppCount = &pCount;
    tracerParams.pphDevices = &phDevices;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetCb_t, Device, pfnGetCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGet,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphDevices);
}

__zedllexport ze_result_t __zecall
zeDeviceGetProperties_Tracing(ze_device_handle_t hDevice,
                              ze_device_properties_t *pDeviceProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetProperties,
                               hDevice,
                               pDeviceProperties);

    ze_device_get_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppDeviceProperties = &pDeviceProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetPropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetPropertiesCb_t, Device, pfnGetPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppDeviceProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetComputeProperties_Tracing(ze_device_handle_t hDevice,
                                     ze_device_compute_properties_t *pComputeProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties,
                               hDevice,
                               pComputeProperties);

    ze_device_get_compute_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppComputeProperties = &pComputeProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetComputePropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetComputePropertiesCb_t, Device, pfnGetComputePropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppComputeProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryProperties_Tracing(ze_device_handle_t hDevice,
                                    uint32_t *pCount,
                                    ze_device_memory_properties_t *pMemProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties,
                               hDevice,
                               pCount,
                               pMemProperties);

    ze_device_get_memory_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppCount = &pCount;
    tracerParams.ppMemProperties = &pMemProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetMemoryPropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetMemoryPropertiesCb_t, Device, pfnGetMemoryPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppMemProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetCacheProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_cache_properties_t *pCacheProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties,
                               hDevice,
                               pCacheProperties);

    ze_device_get_cache_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppCacheProperties = &pCacheProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetCachePropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetCachePropertiesCb_t, Device, pfnGetCachePropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCacheProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetImageProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_image_properties_t *pImageProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties,
                               hDevice,
                               pImageProperties);

    ze_device_get_image_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppImageProperties = &pImageProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetImagePropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetImagePropertiesCb_t, Device, pfnGetImagePropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppImageProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetSubDevices_Tracing(ze_device_handle_t hDevice,
                              uint32_t *pCount,
                              ze_device_handle_t *phSubdevices) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices,
                               hDevice,
                               pCount,
                               phSubdevices);

    ze_device_get_sub_devices_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppCount = &pCount;
    tracerParams.pphSubdevices = &phSubdevices;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetSubDevicesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetSubDevicesCb_t, Device, pfnGetSubDevicesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphSubdevices);
}

__zedllexport ze_result_t __zecall
zeDeviceGetP2PProperties_Tracing(ze_device_handle_t hDevice,
                                 ze_device_handle_t hPeerDevice,
                                 ze_device_p2p_properties_t *pP2PProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties,
                               hDevice,
                               hPeerDevice,
                               pP2PProperties);

    ze_device_get_p2_p_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.phPeerDevice = &hPeerDevice;
    tracerParams.ppP2PProperties = &pP2PProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetP2PPropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetP2PPropertiesCb_t, Device, pfnGetP2PPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phPeerDevice,
                                   *tracerParams.ppP2PProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceCanAccessPeer_Tracing(ze_device_handle_t hDevice,
                              ze_device_handle_t hPeerDevice,
                              ze_bool_t *value) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer,
                               hDevice,
                               hPeerDevice,
                               value);

    ze_device_can_access_peer_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.phPeerDevice = &hPeerDevice;
    tracerParams.pvalue = &value;

    L0::APITracerCallbackDataImp<ze_pfnDeviceCanAccessPeerCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceCanAccessPeerCb_t, Device, pfnCanAccessPeerCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phPeerDevice,
                                   *tracerParams.pvalue);
}

__zedllexport ze_result_t __zecall
zeKernelSetIntermediateCacheConfig_Tracing(ze_kernel_handle_t hKernel,
                                           ze_cache_config_t cacheConfig) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetIntermediateCacheConfig,
                               hKernel,
                               cacheConfig);

    ze_kernel_set_intermediate_cache_config_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pCacheConfig = &cacheConfig;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetIntermediateCacheConfigCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnKernelSetIntermediateCacheConfigCb_t, Kernel, pfnSetIntermediateCacheConfigCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetIntermediateCacheConfig,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pCacheConfig);
}

__zedllexport ze_result_t __zecall
zeDeviceSetLastLevelCacheConfig_Tracing(ze_device_handle_t hDevice,
                                        ze_cache_config_t cacheConfig) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnSetLastLevelCacheConfig,
                               hDevice,
                               cacheConfig);

    ze_device_set_last_level_cache_config_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.pCacheConfig = &cacheConfig;

    L0::APITracerCallbackDataImp<ze_pfnDeviceSetLastLevelCacheConfigCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceSetLastLevelCacheConfigCb_t, Device, pfnSetLastLevelCacheConfigCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnSetLastLevelCacheConfig,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.pCacheConfig);
}

__zedllexport ze_result_t __zecall
zeDeviceGetKernelProperties_Tracing(ze_device_handle_t hDevice,
                                    ze_device_kernel_properties_t *pKernelProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetKernelProperties,
                               hDevice,
                               pKernelProperties);

    ze_device_get_kernel_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppKernelProperties = &pKernelProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetKernelPropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetKernelPropertiesCb_t, Device,
                                  pfnGetKernelPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetKernelProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppKernelProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryAccessProperties_Tracing(ze_device_handle_t hDevice,
                                          ze_device_memory_access_properties_t *pMemAccessProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties,
                               hDevice,
                               pMemAccessProperties);

    ze_device_get_memory_access_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppMemAccessProperties = &pMemAccessProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetMemoryAccessPropertiesCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnDeviceGetMemoryAccessPropertiesCb_t, Device,
                                  pfnGetMemoryAccessPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppMemAccessProperties);
}
