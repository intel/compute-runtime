/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kernel_helpers.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include <algorithm>

namespace NEO {

uint32_t KernelHelper::getMaxWorkGroupCount(uint32_t simd, uint32_t availableThreadCount, uint32_t dssCount, uint32_t availableSlmSize,
                                            uint32_t usedSlmSize, uint32_t maxBarrierCount, uint32_t numberOfBarriers, uint32_t workDim,
                                            const size_t *localWorkSize) {
    if (debugManager.flags.OverrideMaxWorkGroupCount.get() != -1) {
        return static_cast<uint32_t>(debugManager.flags.OverrideMaxWorkGroupCount.get());
    }

    UNRECOVERABLE_IF((workDim == 0) || (workDim > 3));
    UNRECOVERABLE_IF(localWorkSize == nullptr);

    size_t workGroupSize = localWorkSize[0];
    for (uint32_t i = 1; i < workDim; i++) {
        workGroupSize *= localWorkSize[i];
    }

    auto numThreadsPerThreadGroup = static_cast<uint32_t>(Math::divideAndRoundUp(workGroupSize, simd));
    auto maxWorkGroupsCount = availableThreadCount / numThreadsPerThreadGroup;

    if (numberOfBarriers > 0) {
        auto maxWorkGroupsCountDueToBarrierUsage = dssCount * (maxBarrierCount / numberOfBarriers);
        maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToBarrierUsage);
    }

    if (usedSlmSize > 0) {
        auto maxWorkGroupsCountDueToSlm = availableSlmSize / usedSlmSize;
        maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToSlm);
    }

    return maxWorkGroupsCount;
}

KernelHelper::ErrorCode KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(KernelDescriptor::KernelAttributes attributes, Device *device) {
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize();
    if ((attributes.perThreadScratchSize[0] > maxScratchSize) || (attributes.perThreadScratchSize[1] > maxScratchSize)) {
        return KernelHelper::ErrorCode::invalidKernel;
    }
    auto globalMemorySize = device->getDeviceInfo().globalMemSize;
    auto computeUnitsForScratch = device->getDeviceInfo().computeUnitsUsedForScratch;
    auto totalPrivateMemorySize = KernelHelper::getPrivateSurfaceSize(attributes.perHwThreadPrivateMemorySize, computeUnitsForScratch);
    auto totalScratchSize = KernelHelper::getScratchSize(attributes.perThreadScratchSize[0], computeUnitsForScratch);
    auto totalPrivateScratchSize = KernelHelper::getPrivateScratchSize(attributes.perThreadScratchSize[1], computeUnitsForScratch);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                       "computeUnits for each thread: %u\n", computeUnitsForScratch);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                       "perHwThreadPrivateMemorySize: %u\t totalPrivateMemorySize: %lu\n",
                       attributes.perHwThreadPrivateMemorySize, totalPrivateMemorySize);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                       "perHwThreadScratchSize: %u\t totalScratchSize: %lu\n",
                       attributes.perThreadScratchSize[0], totalScratchSize);

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                       "perHwThreadPrivateScratchSize: %u\t totalPrivateScratchSize: %lu\n",
                       attributes.perThreadScratchSize[1], totalPrivateScratchSize);

    if (totalPrivateMemorySize > globalMemorySize ||
        totalScratchSize > globalMemorySize ||
        totalPrivateScratchSize > globalMemorySize) {

        return KernelHelper::ErrorCode::outOfDeviceMemory;
    }
    return KernelHelper::ErrorCode::success;
}

bool KernelHelper::isAnyArgumentPtrByValue(const KernelDescriptor &kernelDescriptor) {
    for (auto &argDescriptor : kernelDescriptor.payloadMappings.explicitArgs) {
        if (argDescriptor.type == NEO::ArgDescriptor::argTValue) {
            for (auto &element : argDescriptor.as<NEO::ArgDescValue>().elements) {
                if (element.isPtr) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace NEO
