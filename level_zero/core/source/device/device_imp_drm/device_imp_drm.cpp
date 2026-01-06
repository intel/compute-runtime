/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

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

bool DeviceImp::queryPeerAccess(NEO::Device &device, NEO::Device &peerDevice, void **handlePtr, uint64_t *handle) {
    if (device.getRootDeviceEnvironment().osInterface) {
        return queryPeerAccessDrm(device, peerDevice, handlePtr, handle);
    }
    return false;
}

} // namespace L0
