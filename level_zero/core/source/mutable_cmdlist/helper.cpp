/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/helper.h"

#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0::MCL {
ze_result_t getBufferGpuAddress(void *buffer, L0::Device *device, NEO::GraphicsAllocation *&outGraphicsAllocation, GpuAddress &outGpuAddress) {
    NEO::GraphicsAllocation *bufferAlloc = nullptr;
    GpuAddress gpuAddress = 0u;
    DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(buffer);
    if (allocData != nullptr) {
        bufferAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        // buffer can be offset SVM value
        gpuAddress = reinterpret_cast<GpuAddress>(buffer);
        if (driverHandle->isRemoteResourceNeeded(buffer, bufferAlloc, allocData, device)) {
            // get GPU base value
            gpuAddress = bufferAlloc->getGpuAddress();
            // calculate possible offset
            size_t offset = reinterpret_cast<GpuAddress>(buffer) - gpuAddress;
            // gpuAddress will get new value from peer allocation
            bufferAlloc = driverHandle->getPeerAllocation(device, allocData, buffer, &gpuAddress, nullptr);
            if (bufferAlloc == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            // add offset to the new peer allocation GPU VA
            gpuAddress += offset;
        }
    } else {
        if (NEO::debugManager.flags.DisableSystemPointerKernelArgument.get() != 1) {
            outGpuAddress = reinterpret_cast<GpuAddress>(buffer);
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    outGraphicsAllocation = bufferAlloc;
    outGpuAddress = gpuAddress;

    return ZE_RESULT_SUCCESS;
}
} // namespace L0::MCL
