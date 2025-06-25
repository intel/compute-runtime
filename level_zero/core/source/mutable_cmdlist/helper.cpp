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
ze_result_t getBufferGpuAddress(void *buffer, L0::Device *device, NEO::GraphicsAllocation *&outGraphicsAllocation, GpuAddress &outGPUAddress) {
    GpuAddress gpuAddress = 0u;
    auto bufferAlloc = device->getDriverHandle()->getDriverSystemMemoryAllocation(buffer,
                                                                                  1u,
                                                                                  device->getRootDeviceIndex(),
                                                                                  &gpuAddress);

    DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(buffer);
    if (driverHandle->isRemoteResourceNeeded(buffer, bufferAlloc, allocData, device)) {
        if (allocData == nullptr) {
            if (NEO::debugManager.flags.DisableSystemPointerKernelArgument.get() != 1) {
                outGPUAddress = reinterpret_cast<GpuAddress>(buffer);
                return ZE_RESULT_SUCCESS;
            } else {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        GpuAddress pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
        size_t offset = reinterpret_cast<size_t>(buffer) - pbase;

        bufferAlloc = driverHandle->getPeerAllocation(device, allocData, buffer, &gpuAddress, nullptr);
        if (bufferAlloc == nullptr) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        gpuAddress += offset;
    }
    outGraphicsAllocation = bufferAlloc;
    outGPUAddress = gpuAddress;

    return ZE_RESULT_SUCCESS;
}
} // namespace L0::MCL
