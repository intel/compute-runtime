/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGet(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_device_handle_t *phDevices) {
    return L0::DriverHandle::fromHandle(hDriver)->getDevice(pCount, phDevices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetSubDevices(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_handle_t *phSubdevices) {
    return L0::Device::fromHandle(hDevice)->getSubDevices(pCount, phSubdevices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetProperties(
    ze_device_handle_t hDevice,
    ze_device_properties_t *pDeviceProperties) {
    return L0::Device::fromHandle(hDevice)->getProperties(pDeviceProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetComputeProperties(
    ze_device_handle_t hDevice,
    ze_device_compute_properties_t *pComputeProperties) {
    return L0::Device::fromHandle(hDevice)->getComputeProperties(pComputeProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetModuleProperties(
    ze_device_handle_t hDevice,
    ze_device_module_properties_t *pKernelProperties) {
    return L0::Device::fromHandle(hDevice)->getKernelProperties(pKernelProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_memory_properties_t *pMemProperties) {
    return L0::Device::fromHandle(hDevice)->getMemoryProperties(pCount, pMemProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetMemoryAccessProperties(
    ze_device_handle_t hDevice,
    ze_device_memory_access_properties_t *pMemAccessProperties) {
    return L0::Device::fromHandle(hDevice)->getMemoryAccessProperties(pMemAccessProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCacheProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_cache_properties_t *pCacheProperties) {
    return L0::Device::fromHandle(hDevice)->getCacheProperties(pCount, pCacheProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetImageProperties(
    ze_device_handle_t hDevice,
    ze_device_image_properties_t *pImageProperties) {
    return L0::Device::fromHandle(hDevice)->getDeviceImageProperties(pImageProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetP2PProperties(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_device_p2p_properties_t *pP2PProperties) {
    return L0::Device::fromHandle(hDevice)->getP2PProperties(hPeerDevice, pP2PProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceCanAccessPeer(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_bool_t *value) {
    return L0::Device::fromHandle(hDevice)->canAccessPeer(hPeerDevice, value);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceSetLastLevelCacheConfig(
    ze_device_handle_t hDevice,
    ze_cache_config_flags_t cacheConfig) {
    return L0::Device::fromHandle(hDevice)->setLastLevelCacheConfig(cacheConfig);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetCommandQueueGroupProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    return L0::Device::fromHandle(hDevice)->getCommandQueueGroupProperties(pCount, pCommandQueueGroupProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetExternalMemoryProperties(
    ze_device_handle_t hDevice,
    ze_device_external_memory_properties_t *pExternalMemoryProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetStatus(
    ze_device_handle_t hDevice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
