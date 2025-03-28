/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/device/device_imp_drm/device_imp_peer.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    if (activeDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == NEO::DriverModelType::wddm) {
            NEO::CommandStreamReceiver *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
            NEO::OsContextWin *context = static_cast<NEO::OsContextWin *>(&csr->getOsContext());
            std::vector<uint8_t> luidData;
            context->getDeviceLuidArray(luidData, ZE_MAX_DEVICE_LUID_SIZE_EXT);
            std::copy_n(luidData.begin(), ZE_MAX_DEVICE_LUID_SIZE_EXT, std::begin(deviceLuidProperties->luid.id));
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

uint32_t DeviceImp::queryDeviceNodeMask() {
    NEO::Device *activeDevice = getActiveDevice();
    if (activeDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == NEO::DriverModelType::wddm) {
            NEO::CommandStreamReceiver *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
            NEO::OsContextWin *context = static_cast<NEO::OsContextWin *>(&csr->getOsContext());
            return context->getDeviceNodeMask();
        }
    }
    return 1;
}

ze_result_t DeviceImp::getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    if (activeDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == NEO::DriverModelType::wddm) {
            pExternalMemoryProperties->imageExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
            pExternalMemoryProperties->imageImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
            pExternalMemoryProperties->memoryAllocationExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
            pExternalMemoryProperties->memoryAllocationImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
        } else {
            pExternalMemoryProperties->imageExportTypes = 0u;
            pExternalMemoryProperties->imageImportTypes = 0u;
            pExternalMemoryProperties->memoryAllocationExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
            pExternalMemoryProperties->memoryAllocationImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::queryFabricStats(DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) {
    NEO::Device *activeDevice = getActiveDevice();
    if (activeDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == NEO::DriverModelType::drm) {
            return queryFabricStatsDrm(this, pPeerDevice, latency, bandwidth);
        }
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
