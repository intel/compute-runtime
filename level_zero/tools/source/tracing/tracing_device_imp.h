/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceGet_Tracing(ze_driver_handle_t hDriver,
                    uint32_t *pCount,
                    ze_device_handle_t *phDevices);

__zedllexport ze_result_t __zecall
zeDeviceGetProperties_Tracing(ze_device_handle_t hDevice,
                              ze_device_properties_t *pDeviceProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetComputeProperties_Tracing(ze_device_handle_t hDevice,
                                     ze_device_compute_properties_t *pComputeProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryProperties_Tracing(ze_device_handle_t hDevice,
                                    uint32_t *pCount,
                                    ze_device_memory_properties_t *pMemProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetCacheProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_cache_properties_t *pCacheProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetImageProperties_Tracing(ze_device_handle_t hDevice,
                                   ze_device_image_properties_t *pImageProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetSubDevices_Tracing(ze_device_handle_t hDevice,
                              uint32_t *pCount,
                              ze_device_handle_t *phSubdevices);

__zedllexport ze_result_t __zecall
zeDeviceGetP2PProperties_Tracing(ze_device_handle_t hDevice,
                                 ze_device_handle_t hPeerDevice,
                                 ze_device_p2p_properties_t *pP2PProperties);

__zedllexport ze_result_t __zecall
zeDeviceCanAccessPeer_Tracing(ze_device_handle_t hDevice,
                              ze_device_handle_t hPeerDevice,
                              ze_bool_t *value);

__zedllexport ze_result_t __zecall
zeKernelSetIntermediateCacheConfig_Tracing(ze_kernel_handle_t hKernel,
                                           ze_cache_config_t cacheConfig);

__zedllexport ze_result_t __zecall
zeDeviceSetLastLevelCacheConfig_Tracing(ze_device_handle_t hDevice,
                                        ze_cache_config_t cacheConfig);

__zedllexport ze_result_t __zecall
zeDeviceGetKernelProperties_Tracing(ze_device_handle_t hDevice,
                                    ze_device_kernel_properties_t *pKernelProperties);

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryAccessProperties_Tracing(ze_device_handle_t hDevice,
                                          ze_device_memory_access_properties_t *pMemAccessProperties);
}
