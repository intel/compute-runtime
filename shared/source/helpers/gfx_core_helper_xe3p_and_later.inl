/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include "metrics_library_api_1_0.h"

namespace NEO {
template <>
uint32_t GfxCoreHelperHw<Family>::getContextGroupContextsCount() const;

template <>
uint32_t GfxCoreHelperHw<Family>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const;

template <>
uint32_t GfxCoreHelperHw<Family>::getContextGroupHpContextsCount(EngineGroupType type, bool hpEngineAvailable) const {
    if (hpEngineAvailable && (type == EngineGroupType::copy || type == NEO::EngineGroupType::linkedCopy)) {
        return 0;
    }
    return std::min(getContextGroupContextsCount() / 2, 4u);
}

template <>
void GfxCoreHelperHw<Family>::adjustCopyEngineRegularContextCount(const size_t enginesCount, uint32_t &contextCount) const {
    if (enginesCount == 2) {
        contextCount = contextCount / 2;
    }
}

template <>
void GfxCoreHelperHw<Family>::initializeDefaultHpCopyEngine(const HardwareInfo &hwInfo) {
    uint32_t hpIndex = 0;

    if (areSecondaryContextsSupported()) {
        auto bscCount = static_cast<uint32_t>(hwInfo.featureTable.ftrBcsInfo.size());
        for (uint32_t i = bscCount - 1; i > 0; i--) {
            if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                hpIndex = i;
                break;
            }
        }
    }

    if (hpIndex == 0 || hwInfo.featureTable.ftrBcsInfo.count() == 1) {
        return;
    }
    hpCopyEngineType = EngineHelpers::getBcsEngineAtIdx(hpIndex);
}

template <>
aub_stream::EngineType GfxCoreHelperHw<Family>::getDefaultHpCopyEngine(const HardwareInfo &hwInfo) const {
    return hpCopyEngineType;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getInternalCopyEngineIndex(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
        return debugManager.flags.ForceBCSForInternalCopyEngine.get();
    }

    constexpr uint32_t defaultInternalCopyEngineIndex = 3u;
    uint64_t supportedBcsBits = hwInfo.featureTable.ftrBcsInfo.to_ullong();
    auto highestAvailableIndex = getMostSignificantSetBitIndex(supportedBcsBits);

    if (areSecondaryContextsSupported() && (getDefaultHpCopyEngine(hwInfo) == EngineHelpers::getBcsEngineAtIdx(highestAvailableIndex))) {
        highestAvailableIndex = getMostSignificantSetBitIndex(supportedBcsBits & ~(1 << highestAvailableIndex));
    }
    return std::min(defaultInternalCopyEngineIndex, highestAvailableIndex);
}

template <>
uint8_t GfxCoreHelperHw<Family>::getBarriersCountFromHasBarriers(uint8_t hasBarriers) const {
    static constexpr uint8_t possibleBarriersCounts[] = {
        0u,  // 0
        1u,  // 1
        2u,  // 2
        4u,  // 3
        8u,  // 4
        16u, // 5
        24u, // 6
        32u, // 7
    };
    return possibleBarriersCounts[hasBarriers];
}

template <>
const EngineInstancesContainer GfxCoreHelperHw<Family>::getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto defaultEngine = getChosenEngineType(hwInfo);
    EngineInstancesContainer engines;

    uint32_t numCcs = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    if (hwInfo.featureTable.flags.ftrCCSNode) {
        for (uint32_t i = 0; i < numCcs; i++) {
            auto engineType = static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS);
            engines.push_back({engineType, EngineUsage::regular});
        }
    }

    if ((debugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) ||
        hwInfo.featureTable.flags.ftrRcsNode) {
        engines.push_back({aub_stream::ENGINE_CCCS, EngineUsage::regular});
    }

    engines.push_back({defaultEngine, EngineUsage::lowPriority});
    engines.push_back({defaultEngine, EngineUsage::internal});

    bool lowPriorityBcsCreated = false;
    bool regularPriorityCreated = false;

    const auto pushDefaultCopyEngineInstances = [&engines, &lowPriorityBcsCreated](aub_stream::EngineType defaultCopyEngineType, bool addLowPriorityEngine) {
        engines.push_back({defaultCopyEngineType, EngineUsage::regular});  // Main copy engine
        engines.push_back({defaultCopyEngineType, EngineUsage::internal}); // Internal usage
        if (addLowPriorityEngine && !lowPriorityBcsCreated) {
            engines.push_back({defaultCopyEngineType, EngineUsage::lowPriority});
            lowPriorityBcsCreated = true;
        }
    };

    if (hwInfo.capabilityTable.blitterOperationsSupported) {
        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            if (const auto &productHelper = rootDeviceEnvironment.getProductHelper(); aub_stream::EngineType::ENGINE_BCS == productHelper.getDefaultCopyEngine()) {
                pushDefaultCopyEngineInstances(aub_stream::ENGINE_BCS, areSecondaryContextsSupported());
                regularPriorityCreated = true;
            }
        }

        auto hpEngine = getDefaultHpCopyEngine(hwInfo);
        uint32_t internalIndex = getInternalCopyEngineIndex(hwInfo);

        for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
            if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                auto engineType = EngineHelpers::getBcsEngineAtIdx(i);
                if (const auto &productHelper = rootDeviceEnvironment.getProductHelper(); engineType == productHelper.getDefaultCopyEngine()) {
                    pushDefaultCopyEngineInstances(engineType, areSecondaryContextsSupported());
                    regularPriorityCreated = true;
                    continue;
                }

                if (areSecondaryContextsSupported() && hpEngine == engineType && regularPriorityCreated) {
                    engines.push_back({engineType, EngineUsage::highPriority});
                } else {
                    engines.push_back({engineType, EngineUsage::regular});
                    regularPriorityCreated = true;

                    if (areSecondaryContextsSupported() && !lowPriorityBcsCreated) {
                        engines.push_back({engineType, EngineUsage::lowPriority});
                        lowPriorityBcsCreated = true;
                    }

                    if (i == internalIndex) {
                        engines.push_back({engineType, EngineUsage::internal});
                    }
                }
            }
        }
    }

    return engines;
};

template <>
EngineGroupType GfxCoreHelperHw<Family>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    if (engineType == aub_stream::ENGINE_CCCS) {
        return EngineGroupType::renderCompute;
    }
    if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
        if (engineUsage == EngineUsage::cooperative) {
            return EngineGroupType::cooperativeCompute;
        }
        return EngineGroupType::compute;
    }
    if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::copy;
    }
    if (engineType >= aub_stream::ENGINE_BCS1 && engineType < aub_stream::ENGINE_BCS1 + hwInfo.featureTable.ftrBcsInfo.size() - 1) {
        return EngineGroupType::linkedCopy;
    }
    UNRECOVERABLE_IF(true);
}

template <>
void GfxCoreHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCCS;
    }
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        return gmmHelper.getL3EnabledMOCS() >> 1;
    }
    return gmmHelper.getUncachedMOCS() >> 1;
}

template <>
aub_stream::MMIOList GfxCoreHelperHw<Family>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    aub_stream::MMIOList mmioList;

    if (compressedBuffersSupported(hwInfo) || compressedImagesSupported(hwInfo)) {
        uint32_t format = gmmHelper.getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

        if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
            format = static_cast<uint32_t>(debugManager.flags.ForceBufferCompressionFormat.get());
        }

        UNRECOVERABLE_IF(format > 0xF);

        mmioList.push_back({0x4148, format}); // [0:3] compression format
    }

    return mmioList;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronization(NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = (fenceType == FenceType::release && !productHelper.isReleaseGlobalFenceInCommandStreamRequired(hwInfo)) ? AdditionalSynchronizationType::none : AdditionalSynchronizationType::fence;
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = static_cast<AdditionalSynchronizationType>(debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get());
    }

    if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::fence) {
        return sizeof(Family::MI_MEM_FENCE);
    } else if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::semaphore) {
        return EncodeSemaphore<Family>::getSizeMiSemaphoreWait();
    }
    return 0;
}

template <>
void MemorySynchronizationCommands<Family>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_MEM_FENCE = typename Family::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = (fenceType == FenceType::release && !productHelper.isReleaseGlobalFenceInCommandStreamRequired(hwInfo)) ? AdditionalSynchronizationType::none : AdditionalSynchronizationType::fence;
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = static_cast<AdditionalSynchronizationType>(debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get());
    }
    if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::fence) {
        MI_MEM_FENCE miMemFence = Family::cmdInitMemFence;
        if (fenceType == NEO::FenceType::acquire) {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE_FENCE);
        } else {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE);
        }
        *reinterpret_cast<MI_MEM_FENCE *>(commandsBuffer) = miMemFence;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(MI_MEM_FENCE));
    } else if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::semaphore) {
        const auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
        bool useSemaphore64bCmd = productHelper.isAvailableSemaphore64(releaseHelper, *rootDeviceEnvironment.getHardwareInfo());
        EncodeSemaphore<Family>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandsBuffer),
                                                        gpuAddress,
                                                        EncodeSemaphore<Family>::invalidHardwareTag,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false, true, false, false, false, useSemaphore64bCmd);
        commandsBuffer = ptrOffset(commandsBuffer, EncodeSemaphore<Family>::getSizeMiSemaphoreWait());
    }
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForAdditionalSynchronization(NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    return (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1 ? 2 : 1) * getSizeForSingleAdditionalSynchronization(fenceType, rootDeviceEnvironment);
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWaFlags(void *barrierCmd) {
}

template <>
inline void MemorySynchronizationCommands<Family>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setDataportFlush(args.hdcPipelineFlush);
    pipeControl.setUnTypedDataPortCacheFlush(args.unTypedDataPortCacheFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);
    pipeControl.setDisableGOSyncWithWalkerPostSync(!args.isWalkerWithProfilingEnqueued);

    if (debugManager.flags.FlushAllCaches.get()) {
        pipeControl.setDataportFlush(true);
        pipeControl.setUnTypedDataPortCacheFlush(true);
        pipeControl.setCompressionControlSurfaceCcsFlush(true);
    }
    if (debugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setDataportFlush(false);
        pipeControl.setUnTypedDataPortCacheFlush(false);
        pipeControl.setCompressionControlSurfaceCcsFlush(false);
    }
    if (debugManager.flags.PcQueueDrainMode.get() != -1) {
        pipeControl.setQueueDrainMode(!!debugManager.flags.PcQueueDrainMode.get());
    }
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment, NEO::PostSyncMode postSyncMode) {
}

template <>
void GfxCoreHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const {
    if (useL1Cache) {
        surfaceState->setL1CacheControlCachePolicy(Family::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);
        if (debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get() != -1) {
            surfaceState->setL1CacheControlCachePolicy(static_cast<typename Family::RENDER_SURFACE_STATE::L1_CACHE_CONTROL>(debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get()));
        }
    }
}

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (hwInfo.featureTable.flags.ftrLocalMemory) {
        if (properties.allocationType == AllocationType::timestampPacketTagBuffer ||
            properties.allocationType == AllocationType::commandBuffer) {
            allocationData.flags.useSystemMemory = false;
        }

        if (properties.allocationType == AllocationType::semaphoreBuffer && !rootDeviceEnvironment.getProductHelper().isAcquireGlobalFenceInDirectSubmissionRequired(hwInfo)) {
            allocationData.flags.useSystemMemory = true;
        }
    }
}

template <>
int32_t GfxCoreHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
size_t GfxCoreHelperHw<Family>::getTimestampPacketAllocatorAlignment() const {
    return MemoryConstants::cacheLineSize;
}

template <>
size_t GfxCoreHelperHw<Family>::getPreemptionAllocationAlignment() const {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = Family::STATE_CONTEXT_DATA_BASE_ADDRESS;
    return STATE_CONTEXT_DATA_BASE_ADDRESS::CONTEXTDATABASEADDRESS::CONTEXTDATABASEADDRESS_ALIGN_SIZE;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMinimalGrfSize() const {
    return 32u;
}

template <>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    const uint32_t threadsPerThreadGroup = calculateNumThreadsPerThreadGroup(simd, defaultMaxGroupSize, grfCount, rootDeviceEnvironment);
    return (threadsPerThreadGroup * simd);
}

template <>
bool GfxCoreHelperHw<Family>::is48ResourceNeededForCmdBuffer() const {
    if (debugManager.flags.Force48bResourcesForCmdBuffer.get() == 1) {
        return true;
    }
    return false;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getAmountOfAllocationsToFill() const {
    if (debugManager.flags.SetAmountOfReusableAllocations.get() != -1) {
        return debugManager.flags.SetAmountOfReusableAllocations.get();
    }
    return 0u;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMaxScratchSize(const NEO::ProductHelper &productHelper) const {
    if (!productHelper.isAvailableExtendedScratch()) {
        return 256 * MemoryConstants::kiloByte;
    }
    return 16 * MemoryConstants::megaByte;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getDefaultSshSize(const ProductHelper &productHelper) const {
    auto defaultSshSize = static_cast<uint32_t>(HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState));
    if (productHelper.isAvailableExtendedScratch()) {
        return 2 * defaultSshSize;
    }
    return defaultSshSize;
}
} // namespace NEO
