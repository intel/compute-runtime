/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetTracing(ze_driver_handle_t hDriver,
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

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetCb_t, Device, pfnGetCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGet,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphDevices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetPropertiesTracing(ze_device_handle_t hDevice,
                             ze_device_properties_t *pDeviceProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetProperties,
                               hDevice,
                               pDeviceProperties);

    ze_device_get_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppDeviceProperties = &pDeviceProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetPropertiesCb_t, Device, pfnGetPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppDeviceProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetComputePropertiesTracing(ze_device_handle_t hDevice,
                                    ze_device_compute_properties_t *pComputeProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties,
                               hDevice,
                               pComputeProperties);

    ze_device_get_compute_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppComputeProperties = &pComputeProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetComputePropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetComputePropertiesCb_t, Device, pfnGetComputePropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppComputeProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryPropertiesTracing(ze_device_handle_t hDevice,
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

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetMemoryPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetMemoryPropertiesCb_t, Device, pfnGetMemoryPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppMemProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCachePropertiesTracing(ze_device_handle_t hDevice,
                                  uint32_t *pCount,
                                  ze_device_cache_properties_t *pCacheProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties,
                               hDevice,
                               pCount,
                               pCacheProperties);

    ze_device_get_cache_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppCount = &pCount;
    tracerParams.ppCacheProperties = &pCacheProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetCachePropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetCachePropertiesCb_t, Device, pfnGetCachePropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppCacheProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetImagePropertiesTracing(ze_device_handle_t hDevice,
                                  ze_device_image_properties_t *pImageProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties,
                               hDevice,
                               pImageProperties);

    ze_device_get_image_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppImageProperties = &pImageProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetImagePropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetImagePropertiesCb_t, Device, pfnGetImagePropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppImageProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetSubDevicesTracing(ze_device_handle_t hDevice,
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

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetSubDevicesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetSubDevicesCb_t, Device, pfnGetSubDevicesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.pphSubdevices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetP2PPropertiesTracing(ze_device_handle_t hDevice,
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

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetP2PPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetP2PPropertiesCb_t, Device, pfnGetP2PPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phPeerDevice,
                                   *tracerParams.ppP2PProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceCanAccessPeerTracing(ze_device_handle_t hDevice,
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

    L0::APITracerCallbackDataImp<ze_pfnDeviceCanAccessPeerCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceCanAccessPeerCb_t, Device, pfnCanAccessPeerCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.phPeerDevice,
                                   *tracerParams.pvalue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetCacheConfigTracing(ze_kernel_handle_t hKernel,
                              ze_cache_config_flags_t flags) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Kernel.pfnSetCacheConfig,
                               hKernel,
                               flags);

    ze_kernel_set_cache_config_params_t tracerParams;
    tracerParams.phKernel = &hKernel;
    tracerParams.pflags = &flags;

    L0::APITracerCallbackDataImp<ze_pfnKernelSetCacheConfigCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnKernelSetCacheConfigCb_t, Kernel, pfnSetCacheConfigCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Kernel.pfnSetCacheConfig,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phKernel,
                                   *tracerParams.pflags);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryAccessPropertiesTracing(ze_device_handle_t hDevice,
                                         ze_device_memory_access_properties_t *pMemAccessProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties,
                               hDevice,
                               pMemAccessProperties);

    ze_device_get_memory_access_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppMemAccessProperties = &pMemAccessProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetMemoryAccessPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetMemoryAccessPropertiesCb_t, Device,
                                  pfnGetMemoryAccessPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppMemAccessProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetModulePropertiesTracing(ze_device_handle_t hDevice,
                                   ze_device_module_properties_t *pModuleProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetModuleProperties,
                               hDevice,
                               pModuleProperties);

    ze_device_get_module_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppModuleProperties = &pModuleProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetModulePropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetModulePropertiesCb_t, Device, pfnGetModulePropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetModuleProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppModuleProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCommandQueueGroupPropertiesTracing(ze_device_handle_t hDevice,
                                              uint32_t *pCount,
                                              ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetCommandQueueGroupProperties,
                               hDevice,
                               pCount,
                               pCommandQueueGroupProperties);

    ze_device_get_command_queue_group_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppCount = &pCount;
    tracerParams.ppCommandQueueGroupProperties = &pCommandQueueGroupProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetCommandQueueGroupPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetCommandQueueGroupPropertiesCb_t, Device, pfnGetCommandQueueGroupPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetCommandQueueGroupProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppCount,
                                   *tracerParams.ppCommandQueueGroupProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetExternalMemoryPropertiesTracing(ze_device_handle_t hDevice,
                                           ze_device_external_memory_properties_t *pExternalMemoryProperties) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetExternalMemoryProperties,
                               hDevice,
                               pExternalMemoryProperties);

    ze_device_get_external_memory_properties_params_t tracerParams;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppExternalMemoryProperties = &pExternalMemoryProperties;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetExternalMemoryPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetExternalMemoryPropertiesCb_t, Device, pfnGetExternalMemoryPropertiesCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetExternalMemoryProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppExternalMemoryProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetStatusTracing(ze_device_handle_t hDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Device.pfnGetStatus,
                               hDevice);

    ze_device_get_status_params_t tracerParams;
    tracerParams.phDevice = &hDevice;

    L0::APITracerCallbackDataImp<ze_pfnDeviceGetStatusCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDeviceGetStatusCb_t, Device, pfnGetStatusCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Device.pfnGetStatus,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDevice);
}
