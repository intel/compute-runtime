/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/device/device_imp_drm/device_imp_peer.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

uint32_t DeviceImp::queryDeviceNodeMask() {
    return 1;
}

ze_result_t DeviceImp::getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) {
    pExternalMemoryProperties->imageExportTypes = 0u;
    pExternalMemoryProperties->imageImportTypes = 0u;
    pExternalMemoryProperties->memoryAllocationExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    pExternalMemoryProperties->memoryAllocationImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) {
    return queryFabricStatsDrm(this, pPeerDevice, latency, bandwidth);
}

} // namespace L0
