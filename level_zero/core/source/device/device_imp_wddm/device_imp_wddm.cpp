/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/windows/os_context_win.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    NEO::CommandStreamReceiver *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
    NEO::OsContextWin *context = static_cast<NEO::OsContextWin *>(&csr->getOsContext());
    std::vector<uint8_t> luidData;
    context->getDeviceLuidArray(luidData, ZE_MAX_DEVICE_LUID_SIZE_EXT);
    std::copy_n(luidData.begin(), ZE_MAX_DEVICE_LUID_SIZE_EXT, std::begin(deviceLuidProperties->luid.id));
    return ZE_RESULT_SUCCESS;
}
uint32_t DeviceImp::queryDeviceNodeMask() {
    NEO::Device *activeDevice = getActiveDevice();
    NEO::CommandStreamReceiver *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
    NEO::OsContextWin *context = static_cast<NEO::OsContextWin *>(&csr->getOsContext());
    return context->getDeviceNodeMask();
}

ze_result_t DeviceImp::getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) {
    pExternalMemoryProperties->imageExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    pExternalMemoryProperties->imageImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32 |
                                                  ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP |
                                                  ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE;
    pExternalMemoryProperties->memoryAllocationExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    pExternalMemoryProperties->memoryAllocationImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32 |
                                                             ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP |
                                                             ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE;

    return ZE_RESULT_SUCCESS;
}

} // namespace L0