/*
 * Copyright (C) 2019-2022 Intel Corporation
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
#include "shared/source/helpers/logical_state_helper.inl"
#include "shared/source/os_interface/hw_info_config.h"

#include "engine_node.h"

namespace NEO {

template <>
inline bool HwHelperHw<Family>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.flags.waDisableFusedThreadScheduling;
    fusedEuDispatchEnabled &= hwInfo.capabilityTable.fusedEuEnabled;

    if (disableEUFusionForKernel)
        fusedEuDispatchEnabled = false;

    if (DebugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (DebugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <>
size_t HwHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 2048;
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
    return featureTable->flags.ftrGpGpuMidThreadLevelPreempt;
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
    return hwInfo.featureTable.flags.ftrLocalMemory;
}

template <>
bool HwHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!DebugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    return false;
}

template <>
bool HwHelperHw<Family>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    return !graphicsAllocation.isCompressionEnabled();
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

    EngineInstancesContainer engines;

    if (defaultEngine == aub_stream::EngineType::ENGINE_CCS && hwInfo.featureTable.flags.ftrCCSNode && !hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt) {
        engines.push_back({aub_stream::ENGINE_CCS, EngineUsage::Regular});
    }

    engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::Regular});
    engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::LowPriority}); // low priority
    engines.push_back({defaultEngine, EngineUsage::Internal});             // internal usage

    if (hwInfo.capabilityTable.blitterOperationsSupported) {
        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::Regular});
        }
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
std::string HwHelperHw<Family>::getExtensions(const HardwareInfo &hwInfo) const {
    std::string extensions;
    extensions += "cl_intel_subgroup_local_block_io ";

    return extensions;
}

template <>
inline void MemorySynchronizationCommands<Family>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

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
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const HardwareInfo &hwInfo) {
    return HwInfoConfig::get(hwInfo.platform.eProductFamily)->pipeControlWARequired(hwInfo);
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierlPriorToPipelineSelectWaRequired(const HardwareInfo &hwInfo) {
    return MemorySynchronizationCommands<Family>::isBarrierWaRequired(hwInfo);
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
        if (properties.allocationType == AllocationType::BUFFER && !properties.flags.preferCompressed && !properties.flags.shareable) {
            allocationData.storageInfo.isLockable = true;
        }
    }
}

template <>
bool HwHelperHw<Family>::forceNonGpuCoherencyWA(bool requiresCoherency) const {
    return false;
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;

template LogicalStateHelper *LogicalStateHelper::create<Family>();
} // namespace NEO
