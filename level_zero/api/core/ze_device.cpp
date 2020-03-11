/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceGet(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_device_handle_t *phDevices) {
    return L0::DriverHandle::fromHandle(hDriver)->getDevice(pCount, phDevices);
}

__zedllexport ze_result_t __zecall
zeDeviceGetSubDevices(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_handle_t *phSubdevices) {
    return L0::Device::fromHandle(hDevice)->getSubDevices(pCount, phSubdevices);
}

__zedllexport ze_result_t __zecall
zeDeviceGetProperties(
    ze_device_handle_t hDevice,
    ze_device_properties_t *pDeviceProperties) {
    return L0::Device::fromHandle(hDevice)->getProperties(pDeviceProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetComputeProperties(
    ze_device_handle_t hDevice,
    ze_device_compute_properties_t *pComputeProperties) {
    return L0::Device::fromHandle(hDevice)->getComputeProperties(pComputeProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetKernelProperties(
    ze_device_handle_t hDevice,
    ze_device_kernel_properties_t *pKernelProperties) {
    return L0::Device::fromHandle(hDevice)->getKernelProperties(pKernelProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_memory_properties_t *pMemProperties) {
    return L0::Device::fromHandle(hDevice)->getMemoryProperties(pCount, pMemProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryAccessProperties(
    ze_device_handle_t hDevice,
    ze_device_memory_access_properties_t *pMemAccessProperties) {
    return L0::Device::fromHandle(hDevice)->getMemoryAccessProperties(pMemAccessProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetCacheProperties(
    ze_device_handle_t hDevice,
    ze_device_cache_properties_t *pCacheProperties) {
    return L0::Device::fromHandle(hDevice)->getCacheProperties(pCacheProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetImageProperties(
    ze_device_handle_t hDevice,
    ze_device_image_properties_t *pImageProperties) {
    return L0::Device::fromHandle(hDevice)->getDeviceImageProperties(pImageProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceGetP2PProperties(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_device_p2p_properties_t *pP2PProperties) {
    return L0::Device::fromHandle(hDevice)->getP2PProperties(hPeerDevice, pP2PProperties);
}

__zedllexport ze_result_t __zecall
zeDeviceCanAccessPeer(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_bool_t *value) {
    return L0::Device::fromHandle(hDevice)->canAccessPeer(hPeerDevice, value);
}

__zedllexport ze_result_t __zecall
zeDeviceSetLastLevelCacheConfig(
    ze_device_handle_t hDevice,
    ze_cache_config_t cacheConfig) {
    return L0::Device::fromHandle(hDevice)->setLastLevelCacheConfig(cacheConfig);
}

} // extern "C"
