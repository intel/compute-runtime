/*
 * Copyright (C) 2019-2025 Intel Corporation
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
#include "shared/source/helpers/hw_info.h"
#include "shared/source/program/sync_buffer_handler.h"

namespace NEO {

uint32_t KernelHelper::getMaxWorkGroupCount(Device &device, uint16_t numGrfRequired, uint8_t simdSize, uint8_t barrierCount, uint32_t alignedSlmSize, uint32_t workDim, const size_t *localWorkSize,
                                            EngineGroupType engineGroupType, bool implicitScalingEnabled, bool forceSingleTileQuery) {
    uint32_t numSubDevicesForExecution = 1;

    auto deviceBitfield = device.getDeviceBitfield();
    if (!forceSingleTileQuery && implicitScalingEnabled) {
        numSubDevicesForExecution = static_cast<uint32_t>(deviceBitfield.count());
    }

    return KernelHelper::getMaxWorkGroupCount(device.getRootDeviceEnvironment(), numGrfRequired, simdSize, barrierCount, numSubDevicesForExecution, alignedSlmSize, workDim, localWorkSize, engineGroupType);
}

uint32_t KernelHelper::getMaxWorkGroupCount(const RootDeviceEnvironment &rootDeviceEnvironment, uint16_t numGrfRequired, uint8_t simdSize, uint8_t barrierCount,
                                            uint32_t numSubDevices, uint32_t usedSlmSize, uint32_t workDim, const size_t *localWorkSize, EngineGroupType engineGroupType) {
    if (debugManager.flags.OverrideMaxWorkGroupCount.get() != -1) {
        return static_cast<uint32_t>(debugManager.flags.OverrideMaxWorkGroupCount.get());
    }

    auto &helper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();

    auto dssCount = hwInfo.gtSystemInfo.DualSubSliceCount;
    if (dssCount == 0) {
        dssCount = hwInfo.gtSystemInfo.SubSliceCount;
    }

    auto availableThreadCount = helper.calculateAvailableThreadCount(hwInfo, numGrfRequired, rootDeviceEnvironment);
    auto availableSlmSize = static_cast<uint32_t>(dssCount * MemoryConstants::kiloByte * hwInfo.capabilityTable.maxProgrammableSlmSize);
    auto maxBarrierCount = static_cast<uint32_t>(helper.getMaxBarrierRegisterPerSlice());

    UNRECOVERABLE_IF((workDim == 0) || (workDim > 3));
    UNRECOVERABLE_IF(localWorkSize == nullptr);

    size_t workGroupSize = localWorkSize[0];
    for (uint32_t i = 1; i < workDim; i++) {
        workGroupSize *= localWorkSize[i];
    }
    UNRECOVERABLE_IF(workGroupSize == 0);
    auto numThreadsPerThreadGroup = static_cast<uint32_t>(Math::divideAndRoundUp(workGroupSize, simdSize));
    auto maxWorkGroupsCount = availableThreadCount / numThreadsPerThreadGroup;
    if (barrierCount > 0 || usedSlmSize > 0) {
        helper.alignThreadGroupCountToDssSize(maxWorkGroupsCount, dssCount, availableThreadCount / dssCount, numThreadsPerThreadGroup);
        if (barrierCount > 0) {
            auto maxWorkGroupsCountDueToBarrierUsage = dssCount * (maxBarrierCount / barrierCount);
            maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToBarrierUsage);
        }
        if (usedSlmSize > 0) {
            auto maxWorkGroupsCountDueToSlm = availableSlmSize / usedSlmSize;
            maxWorkGroupsCount = std::min(maxWorkGroupsCount, maxWorkGroupsCountDueToSlm);
        }
    }

    maxWorkGroupsCount = helper.adjustMaxWorkGroupCount(maxWorkGroupsCount, engineGroupType, rootDeviceEnvironment);

    if (!helper.singleTileExecImplicitScalingRequired(true)) {
        maxWorkGroupsCount *= numSubDevices;
    }

    return maxWorkGroupsCount;
}

KernelHelper::ErrorCode KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(KernelDescriptor::KernelAttributes attributes, Device *device) {
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &productHelper = device->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);
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
                       "global memory size: %llu\n", globalMemorySize);

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

std::pair<GraphicsAllocation *, size_t> KernelHelper::getRegionGroupBarrierAllocationOffset(Device &device, const size_t threadGroupCount, const size_t localRegionSize) {
    device.allocateSyncBufferHandler();

    size_t size = KernelHelper::getRegionGroupBarrierSize(threadGroupCount, localRegionSize);

    return device.syncBufferHandler->obtainAllocationAndOffset(size);
}

std::pair<GraphicsAllocation *, size_t> KernelHelper::getSyncBufferAllocationOffset(Device &device, const size_t requestedNumberOfWorkgroups) {
    device.allocateSyncBufferHandler();

    size_t requiredSize = KernelHelper::getSyncBufferSize(requestedNumberOfWorkgroups);

    return device.syncBufferHandler->obtainAllocationAndOffset(requiredSize);
}

} // namespace NEO
