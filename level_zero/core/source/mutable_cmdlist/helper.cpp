/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace L0::MCL {
ze_result_t getBufferGpuAddress(void *buffer, L0::Device *device, NEO::GraphicsAllocation *&outGraphicsAllocation, GpuAddress &outGpuAddress, uint32_t &outAllocId) {
    NEO::GraphicsAllocation *bufferAlloc = nullptr;
    GpuAddress gpuAddress = 0u;
    DriverHandle *driverHandle = device->getDriverHandle();
    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(buffer);
    uint32_t allocId = 0;
    if (allocData != nullptr) {
        bufferAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        // buffer can be offset SVM value
        gpuAddress = reinterpret_cast<GpuAddress>(buffer);
        allocId = allocData->getAllocId();
        if (driverHandle->isRemoteResourceNeeded(*allocData, device)) {
            // get GPU base value of the source allocation
            uint64_t pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
            // calculate possible offset from the source allocation base
            size_t offset = reinterpret_cast<GpuAddress>(buffer) - pbase;
            // gpuAddress will get base value of the peer allocation
            bufferAlloc = driverHandle->getPeerAllocation(device, allocData, reinterpret_cast<void *>(pbase), &gpuAddress, nullptr, false);
            if (bufferAlloc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            // add offset to the new peer allocation GPU VA
            gpuAddress += offset;
        }
    } else {
        if (NEO::debugManager.flags.DisableSystemPointerKernelArgument.get() != 1) {
            gpuAddress = reinterpret_cast<GpuAddress>(buffer);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    outGraphicsAllocation = bufferAlloc;
    outGpuAddress = gpuAddress;
    outAllocId = allocId;

    return ZE_RESULT_SUCCESS;
}
} // namespace L0::MCL
