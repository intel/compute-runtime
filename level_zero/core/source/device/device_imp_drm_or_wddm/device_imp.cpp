/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    if (activeDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == NEO::DriverModelType::WDDM) {
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
        if (driverType == NEO::DriverModelType::WDDM) {
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
        if (driverType == NEO::DriverModelType::WDDM) {
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

        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

} // namespace L0