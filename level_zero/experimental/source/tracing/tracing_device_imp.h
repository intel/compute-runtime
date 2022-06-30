/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetTracing(ze_driver_handle_t hDriver,
                   uint32_t *pCount,
                   ze_device_handle_t *phDevices);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetPropertiesTracing(ze_device_handle_t hDevice,
                             ze_device_properties_t *pDeviceProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetComputePropertiesTracing(ze_device_handle_t hDevice,
                                    ze_device_compute_properties_t *pComputeProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryPropertiesTracing(ze_device_handle_t hDevice,
                                   uint32_t *pCount,
                                   ze_device_memory_properties_t *pMemProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCachePropertiesTracing(ze_device_handle_t hDevice,
                                  uint32_t *pCount,
                                  ze_device_cache_properties_t *pCacheProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetImagePropertiesTracing(ze_device_handle_t hDevice,
                                  ze_device_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetSubDevicesTracing(ze_device_handle_t hDevice,
                             uint32_t *pCount,
                             ze_device_handle_t *phSubdevices);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetP2PPropertiesTracing(ze_device_handle_t hDevice,
                                ze_device_handle_t hPeerDevice,
                                ze_device_p2p_properties_t *pP2PProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceCanAccessPeerTracing(ze_device_handle_t hDevice,
                             ze_device_handle_t hPeerDevice,
                             ze_bool_t *value);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetCacheConfigTracing(ze_kernel_handle_t hKernel,
                              ze_cache_config_flags_t flags);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryAccessPropertiesTracing(ze_device_handle_t hDevice,
                                         ze_device_memory_access_properties_t *pMemAccessProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetModulePropertiesTracing(ze_device_handle_t hDevice,
                                   ze_device_module_properties_t *pModuleProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCommandQueueGroupPropertiesTracing(ze_device_handle_t hDevice,
                                              uint32_t *pCount,
                                              ze_command_queue_group_properties_t *pCommandQueueGroupProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetExternalMemoryPropertiesTracing(ze_device_handle_t hDevice,
                                           ze_device_external_memory_properties_t *pExternalMemoryProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetStatusTracing(ze_device_handle_t hDevice);
}
