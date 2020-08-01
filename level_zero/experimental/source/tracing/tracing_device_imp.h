/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGet_Tracing(ze_driver_handle_t hDriver,
                    uint32_t *pCount,
                    ze_device_handle_t *phDevices);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetProperties_Tracing(ze_device_handle_t hDevice,
                              ze_device_properties_t *pDeviceProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetComputeProperties_Tracing(ze_device_handle_t hDevice,
                                     ze_device_compute_properties_t *pComputeProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryProperties_Tracing(ze_device_handle_t hDevice,
                                    uint32_t *pCount,
                                    ze_device_memory_properties_t *pMemProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCacheProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_cache_properties_t *pCacheProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetImageProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetSubDevices_Tracing(ze_device_handle_t hDevice,
                              uint32_t *pCount,
                              ze_device_handle_t *phSubdevices);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetP2PProperties_Tracing(ze_device_handle_t hDevice,
                                 ze_device_handle_t hPeerDevice,
                                 ze_device_p2p_properties_t *pP2PProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceCanAccessPeer_Tracing(ze_device_handle_t hDevice,
                              ze_device_handle_t hPeerDevice,
                              ze_bool_t *value);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetIntermediateCacheConfig_Tracing(ze_kernel_handle_t hKernel,
                                           ze_cache_config_t cacheConfig);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceSetLastLevelCacheConfig_Tracing(ze_device_handle_t hDevice,
                                        ze_cache_config_t cacheConfig);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetKernelProperties_Tracing(ze_device_handle_t hDevice,
                                    ze_device_kernel_properties_t *pKernelProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryAccessProperties_Tracing(ze_device_handle_t hDevice,
                                          ze_device_memory_access_properties_t *pMemAccessProperties);
}
