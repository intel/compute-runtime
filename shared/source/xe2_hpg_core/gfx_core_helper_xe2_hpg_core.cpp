/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/aub_mapper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

using Family = NEO::Xe2HpgCoreFamily;

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_pvc_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::none;

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
uint32_t GfxCoreHelperHw<Family>::getMinimalSIMDSize() const {
    return 16u;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        return gmmHelper.getL3EnabledMOCS() >> 1;
    }
    return gmmHelper.getUncachedMOCS() >> 1;
}

template <>
const StackVec<size_t, 3> GfxCoreHelperHw<Family>::getDeviceSubGroupSizes() const {
    return {16, 32};
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
        EncodeSemaphore<Family>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandsBuffer),
                                                        gpuAddress,
                                                        EncodeSemaphore<Family>::invalidHardwareTag,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false, true, false, false, false);
        commandsBuffer = ptrOffset(commandsBuffer, EncodeSemaphore<Family>::getSizeMiSemaphoreWait());
    }
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return true;
    }
    return false;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForAdditionalSynchronization(NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    return (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1 ? 2 : 1) * getSizeForSingleAdditionalSynchronization(fenceType, rootDeviceEnvironment);
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWaFlags(void *barrierCmd) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setCommandStreamerStallEnable(true);
    pipeControl.setDataportFlush(true);
    pipeControl.setUnTypedDataPortCacheFlush(true);
}

template <>
inline void MemorySynchronizationCommands<Family>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setDataportFlush(args.hdcPipelineFlush);
    pipeControl.setUnTypedDataPortCacheFlush(args.unTypedDataPortCacheFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);

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
        if (properties.allocationType == AllocationType::timestampPacketTagBuffer) {
            allocationData.flags.useSystemMemory = false;
        }

        if (properties.allocationType == AllocationType::commandBuffer ||
            properties.allocationType == AllocationType::ringBuffer) {
            allocationData.flags.useSystemMemory = false;
            allocationData.flags.requiresCpuAccess = true;
        }

        if (properties.allocationType == AllocationType::semaphoreBuffer) {
            if (rootDeviceEnvironment.getProductHelper().isAcquireGlobalFenceInDirectSubmissionRequired(hwInfo)) {
                allocationData.flags.useSystemMemory = false;
            } else {
                allocationData.flags.useSystemMemory = true;
            }
            allocationData.flags.requiresCpuAccess = true;
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
uint32_t GfxCoreHelperHw<Family>::overrideMaxWorkGroupSize(uint32_t maxWG) const {
    return std::min(maxWG, 2048u);
}

template <>
uint32_t GfxCoreHelperHw<Family>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    uint32_t numThreadsPerThreadGroup = getThreadsPerWG(simd, totalWorkItems);
    if (debugManager.flags.RemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroup.get() == 1) {
        return numThreadsPerThreadGroup;
    }

    uint32_t maxThreadsPerThreadGroup = 32u;
    // driver limit maxWorkgroupSize to 1024 (NEO-11881) so for simt 32 the max threads per thread group is 32
    if ((grfCount != GrfConfig::largeGrfNumber && (simd == 16u)) || isSimd1(simd)) {
        maxThreadsPerThreadGroup = 64u;
    }
    numThreadsPerThreadGroup = std::min(numThreadsPerThreadGroup, maxThreadsPerThreadGroup);
    DEBUG_BREAK_IF(numThreadsPerThreadGroup * simd > CommonConstants::maxWorkgroupSize);
    return numThreadsPerThreadGroup;
}

template <>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    const uint32_t threadsPerThreadGroup = calculateNumThreadsPerThreadGroup(simd, defaultMaxGroupSize, grfCount, rootDeviceEnvironment);
    return (threadsPerThreadGroup * simd);
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe2HPG);
}

template <>
void MemorySynchronizationCommands<Family>::setStallingBarrier(void *commandsBuffer, PipeControlArgs &args) {
    using RESOURCE_BARRIER = typename Family::RESOURCE_BARRIER;

    auto resourceBarrier = Family::cmdInitResourceBarrier;
    resourceBarrier.setBarrierType(RESOURCE_BARRIER::BARRIER_TYPE::BARRIER_TYPE_IMMEDIATE);
    resourceBarrier.setWaitStage(RESOURCE_BARRIER::WAIT_STAGE::WAIT_STAGE_TOP);
    resourceBarrier.setSignalStage(RESOURCE_BARRIER::SIGNAL_STAGE::SIGNAL_STAGE_GPGPU);
    *reinterpret_cast<RESOURCE_BARRIER *>(commandsBuffer) = resourceBarrier;
}

} // namespace NEO

namespace NEO {
template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
