/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

namespace L0 {
ze_result_t ZE_APICALL zeDeviceGet(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_device_handle_t *phDevices);

ze_result_t ZE_APICALL zeDeviceGetSubDevices(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_handle_t *phSubdevices);

ze_result_t ZE_APICALL zeDeviceGetProperties(
    ze_device_handle_t hDevice,
    ze_device_properties_t *pDeviceProperties);

ze_result_t ZE_APICALL zeDeviceGetComputeProperties(
    ze_device_handle_t hDevice,
    ze_device_compute_properties_t *pComputeProperties);

ze_result_t ZE_APICALL zeDeviceGetModuleProperties(
    ze_device_handle_t hDevice,
    ze_device_module_properties_t *pKernelProperties);

ze_result_t ZE_APICALL zeDeviceGetMemoryProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_memory_properties_t *pMemProperties);

ze_result_t ZE_APICALL zeDeviceGetMemoryAccessProperties(
    ze_device_handle_t hDevice,
    ze_device_memory_access_properties_t *pMemAccessProperties);

ze_result_t ZE_APICALL zeDeviceGetCacheProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_cache_properties_t *pCacheProperties);

ze_result_t ZE_APICALL zeDeviceGetImageProperties(
    ze_device_handle_t hDevice,
    ze_device_image_properties_t *pImageProperties);

ze_result_t ZE_APICALL zeDeviceGetP2PProperties(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_device_p2p_properties_t *pP2PProperties);

ze_result_t ZE_APICALL zeDeviceCanAccessPeer(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_bool_t *value);

ze_result_t ZE_APICALL zeDeviceGetCommandQueueGroupProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_command_queue_group_properties_t *pCommandQueueGroupProperties);

ze_result_t ZE_APICALL zeDeviceGetExternalMemoryProperties(
    ze_device_handle_t hDevice,
    ze_device_external_memory_properties_t *pExternalMemoryProperties);

ze_result_t ZE_APICALL zeDeviceGetStatus(
    ze_device_handle_t hDevice);

ze_result_t ZE_APICALL zeDeviceGetGlobalTimestamps(
    ze_device_handle_t hDevice,
    uint64_t *hostTimestamp,
    uint64_t *deviceTimestamp);

ze_result_t ZE_APICALL zeDeviceReserveCacheExt(
    ze_device_handle_t hDevice,
    size_t cacheLevel,
    size_t cacheReservationSize);

ze_result_t ZE_APICALL zeDeviceSetCacheAdviceExt(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t regionSize,
    ze_cache_ext_region_t cacheRegion);

ze_result_t ZE_APICALL zeDevicePciGetPropertiesExt(
    ze_device_handle_t hDevice,
    ze_pci_ext_properties_t *pPciProperties);

ze_result_t ZE_APICALL zeDeviceGetRootDevice(
    ze_device_handle_t hDevice,
    ze_device_handle_t *phRootDevice);

ze_result_t ZE_APICALL zeDeviceImportExternalSemaphoreExt(
    ze_device_handle_t hDevice,
    const ze_external_semaphore_ext_desc_t *desc,
    ze_external_semaphore_ext_handle_t *phSemaphore);

ze_result_t ZE_APICALL zeDeviceReleaseExternalSemaphoreExt(
    ze_external_semaphore_ext_handle_t hSemaphore);

ze_result_t ZE_APICALL zeDeviceGetVectorWidthPropertiesExt(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_vector_width_properties_ext_t *pVectorWidthProperties);

ze_result_t ZE_APICALL zeDeviceSynchronize(ze_device_handle_t hDevice);

ze_result_t ZE_APICALL zeDeviceGetPriorityLevels(
    ze_device_handle_t hDevice,
    int32_t *lowestPriority,
    int32_t *highestPriority);

ze_result_t ZE_APICALL zeDeviceGetAggregatedCopyOffloadIncrementValue(
    ze_device_handle_t hDevice,
    uint32_t *incrementValue);

ze_result_t ZE_APICALL zeDeviceGetRuntimeRequirements(
    ze_device_handle_t hDevice,
    const void *pObjDesc,
    size_t *pSize,
    char *pRequirements);

ze_result_t ZE_APICALL zeDeviceGetRuntimeRequirementsKey(
    ze_device_handle_t hDevice,
    const char **pKey);

ze_result_t ZE_APICALL zeDeviceValidateRuntimeRequirements(
    ze_device_handle_t hDevice,
    const char *pRequirements,
    ze_validate_runtime_requirements_output_t *pOut);

ze_result_t ZE_APICALL zeDeviceGetCounterBasedEventMaxValue(
    ze_device_handle_t hDevice,
    uint64_t *maxValue);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGet(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_device_handle_t *phDevices);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetSubDevices(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_handle_t *phSubdevices);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetProperties(
    ze_device_handle_t hDevice,
    ze_device_properties_t *pDeviceProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetComputeProperties(
    ze_device_handle_t hDevice,
    ze_device_compute_properties_t *pComputeProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetModuleProperties(
    ze_device_handle_t hDevice,
    ze_device_module_properties_t *pModuleProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetCommandQueueGroupProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_command_queue_group_properties_t *pCommandQueueGroupProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetMemoryProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_memory_properties_t *pMemProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetMemoryAccessProperties(
    ze_device_handle_t hDevice,
    ze_device_memory_access_properties_t *pMemAccessProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetCacheProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_cache_properties_t *pCacheProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetImageProperties(
    ze_device_handle_t hDevice,
    ze_device_image_properties_t *pImageProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetExternalMemoryProperties(
    ze_device_handle_t hDevice,
    ze_device_external_memory_properties_t *pExternalMemoryProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetP2PProperties(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_device_p2p_properties_t *pP2PProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceCanAccessPeer(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_bool_t *value);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetStatus(
    ze_device_handle_t hDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetGlobalTimestamps(
    ze_device_handle_t hDevice,
    uint64_t *hostTimestamp,
    uint64_t *deviceTimestamp);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceReserveCacheExt(
    ze_device_handle_t hDevice,
    size_t cacheLevel,
    size_t cacheReservationSize);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceSetCacheAdviceExt(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t regionSize,
    ze_cache_ext_region_t cacheRegion);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDevicePciGetPropertiesExt(
    ze_device_handle_t hDevice,
    ze_pci_ext_properties_t *pPciProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceImportExternalSemaphoreExt(
    ze_device_handle_t hDevice,
    const ze_external_semaphore_ext_desc_t *desc,
    ze_external_semaphore_ext_handle_t *phSemaphore);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceReleaseExternalSemaphoreExt(
    ze_external_semaphore_ext_handle_t hSemaphore);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetVectorWidthPropertiesExt(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_vector_width_properties_ext_t *pVectorWidthProperties);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceSynchronize(ze_device_handle_t hDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetPriorityLevels(
    ze_device_handle_t hDevice,
    int32_t *lowestPriority,
    int32_t *highestPriority);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetAggregatedCopyOffloadIncrementValue(
    ze_device_handle_t hDevice,
    uint32_t *incrementValue);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetRuntimeRequirements(
    ze_device_handle_t hDevice,
    const void *pObjDesc,
    size_t *pSize,
    char *pRequirements);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetRuntimeRequirementsKey(
    ze_device_handle_t hDevice,
    const char **pKey);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceValidateRuntimeRequirements(
    ze_device_handle_t hDevice,
    const char *pRequirements,
    ze_validate_runtime_requirements_output_t *pOut);

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceGetCounterBasedEventMaxValue(
    ze_device_handle_t hDevice,
    uint64_t *maxValue);

} // extern "C"
