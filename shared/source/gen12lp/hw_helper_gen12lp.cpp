/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/aub_mapper.h"
#include "shared/source/gen12lp/hw_cmds.h"

using Family = NEO::TGLLPFamily;

#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_bdw_and_later.inl"
#include "shared/source/helpers/hw_helper_tgllp_and_later.inl"
#include "shared/source/os_interface/hw_info_config.h"

#include "engine_node.h"

namespace NEO {

template <>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 2048;
    caps->image3DMaxWidth = 2048;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
}

template <>
bool HwHelperHw<Family>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const {
    return isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwHelperHw<Family>::isWaDisableRccRhwoOptimizationRequired() const {
    return true;
}

template <>
bool HwHelperHw<Family>::isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const {
    return featureTable->ftrGpGpuMidThreadLevelPreempt;
}

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    /* For ICL+ maxThreadCount equals (EUCount * 8).
     ThreadCount/EUCount=7 is no longer valid, so we have to force 8 in below formula.
     This is required to allocate enough scratch space. */
    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template <>
bool HwHelperHw<Family>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.ftrLocalMemory;
}

template <>
bool HwHelperHw<Family>::isBufferSizeSuitableForRenderCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!DebugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    return false;
}

template <>
bool HwHelperHw<Family>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    if (graphicsAllocation.getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) {
        return false;
    }
    return true;
}

template <>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) const {
    if (HwInfoConfig::get(hwInfo->platform.eProductFamily)->imagePitchAlignmentWARequired(*hwInfo)) {
        return 64u;
    }
    return 4u;
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen12);
}

template <>
const EngineInstancesContainer HwHelperHw<Family>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);

    EngineInstancesContainer engines = {
        {aub_stream::ENGINE_RCS, EngineUsage::Regular},
        {aub_stream::ENGINE_RCS, EngineUsage::LowPriority}, // low priority
        {defaultEngine, EngineUsage::Internal},             // internal usage
    };

    if (defaultEngine == aub_stream::EngineType::ENGINE_CCS && hwInfo.featureTable.ftrCCSNode && !hwInfo.featureTable.ftrGpGpuMidThreadLevelPreempt) {
        engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::Regular});
    }

    if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
        engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::Regular});
    }

    return engines;
};

template <>
EngineGroupType HwHelperHw<Family>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    switch (engineType) {
    case aub_stream::ENGINE_RCS:
        return EngineGroupType::RenderCompute;
    case aub_stream::ENGINE_CCS:
        return EngineGroupType::Compute;
    case aub_stream::ENGINE_BCS:
        return EngineGroupType::Copy;
    default:
        UNRECOVERABLE_IF(true);
    }
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->pipeControlWARequired(hwInfo)) {
        PIPE_CONTROL cmd = Family::cmdInitPipeControl;
        cmd.setCommandStreamerStallEnable(true);
        auto pipeControl = static_cast<Family::PIPE_CONTROL *>(commandStream.getSpace(sizeof(PIPE_CONTROL)));
        *pipeControl = cmd;
    }
}

template <>
std::string HwHelperHw<Family>::getExtensions() const {
    return "cl_intel_subgroup_local_block_io ";
}

template <>
inline void MemorySynchronizationCommands<Family>::setPipeControlExtraProperties(PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pipeControl.setHdcPipelineFlush(true);
    }
    if (DebugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setHdcPipelineFlush(false);
    }
}

template <>
void MemorySynchronizationCommands<Family>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
    args.constantCacheInvalidationEnable = false;
}

template <>
bool HwHelperHw<Family>::useOnlyGlobalTimestamps() const {
    return true;
}

template <>
uint32_t HwHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        if (DebugManager.flags.ForceL1Caching.get() != 1) {
            l1enabled = false;
        }

        if (l1enabled) {
            return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;
        } else {
            return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
        }
    }

    return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
}

template <>
bool MemorySynchronizationCommands<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return HwInfoConfig::get(hwInfo.platform.eProductFamily)->pipeControlWARequired(hwInfo);
}

template <>
bool MemorySynchronizationCommands<Family>::isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo) {
    return MemorySynchronizationCommands<Family>::isPipeControlWArequired(hwInfo);
}

template <>
void HwHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed) {
        if (GraphicsAllocation::isCpuAccessRequired(properties.allocationType)) {
            allocationData.flags.useSystemMemory = true;
        }
    }
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->isStorageInfoAdjustmentRequired()) {
        if (properties.allocationType == GraphicsAllocation::AllocationType::BUFFER) {
            allocationData.storageInfo.isLockable = true;
        }
    }
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
