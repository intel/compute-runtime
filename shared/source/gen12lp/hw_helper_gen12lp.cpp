/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/aub_mapper.h"
#include "shared/source/gen12lp/hw_cmds.h"

using Family = NEO::TGLLPFamily;

#include "shared/source/gen12lp/helpers_gen12lp.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_bdw_plus.inl"
#include "shared/source/helpers/hw_helper_tgllp_plus.inl"
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
    return Gen12LPHelpers::isOffsetToSkipSetFFIDGPWARequired(hwInfo);
}

template <>
bool HwHelperHw<Family>::is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const {
    return Gen12LPHelpers::is3DPipelineSelectWARequired(hwInfo);
}

template <>
bool HwHelperHw<Family>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return Gen12LPHelpers::isForceEmuInt32DivRemSPWARequired(hwInfo);
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
    return Gen12LPHelpers::isLocalMemoryEnabled(hwInfo);
}

template <>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
uint32_t HwHelperHw<Family>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP) {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x1;
        case REVISION_C:
            return 0x3;
        }
    } else if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_ROCKETLAKE) {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x1;
        case REVISION_C:
            return 0x4;
        }
    }
    return Gen12LPHelpers::getHwRevIdFromStepping(stepping, hwInfo);
}

template <>
uint32_t HwHelperHw<Family>::getSteppingFromHwRevId(uint32_t hwRevId, const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP) {
        switch (hwRevId) {
        case 0x0:
            return REVISION_A0;
        case 0x1:
            return REVISION_B;
        case 0x3:
            return REVISION_C;
        }
    } else if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_ROCKETLAKE) {
        switch (hwRevId) {
        case 0x0:
            return REVISION_A0;
        case 0x1:
            return REVISION_B;
        case 0x4:
            return REVISION_C;
        }
    }
    return Gen12LPHelpers::getSteppingFromHwRevId(hwRevId, hwInfo);
}

template <>
bool HwHelperHw<Family>::obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const {
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
void HwHelperHw<Family>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
    HwHelper &hwHelper = HwHelper::get(pHwInfo->platform.eRenderCoreFamily);
    if (pHwInfo->platform.eProductFamily == IGFX_TIGERLAKE_LP && hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *pHwInfo)) {
        //stepping A devices - turn off coherency
        coherencyFlag = false;
    }

    Gen12LPHelpers::adjustCoherencyFlag(pHwInfo->platform.eProductFamily, coherencyFlag);
}

template <>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) {
    if (Gen12LPHelpers::imagePitchAlignmentWaRequired(hwInfo->platform.eProductFamily)) {
        HwHelper &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
        if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo)) {
            return 64u;
        }
        return 4u;
    }
    return 4u;
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen12);
}

template <>
const HwHelper::EngineInstancesContainer HwHelperHw<Family>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);

    EngineInstancesContainer engines = {
        aub_stream::ENGINE_RCS,
        aub_stream::ENGINE_RCS, // low priority
        defaultEngine           // internal usage
    };

    if (defaultEngine == aub_stream::EngineType::ENGINE_CCS && hwInfo.featureTable.ftrCCSNode && !hwInfo.featureTable.ftrGpGpuMidThreadLevelPreempt) {
        engines.push_back(aub_stream::ENGINE_CCS);
    }

    if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
        engines.push_back(aub_stream::ENGINE_BCS);
    }

    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig->isEvenContextCountRequired() && engines.size() & 1) {
        engines.push_back(aub_stream::ENGINE_RCS);
    }

    return engines;
};

template <>
void HwHelperHw<Family>::addEngineToEngineGroup(std::vector<std::vector<EngineControl>> &engineGroups,
                                                EngineControl &engine, const HardwareInfo &hwInfo) const {
    if (engine.getEngineType() == aub_stream::ENGINE_RCS) {
        engineGroups[static_cast<uint32_t>(EngineGroupType::RenderCompute)].push_back(engine);
    }
    if (engine.getEngineType() == aub_stream::ENGINE_CCS) {
        engineGroups[static_cast<uint32_t>(EngineGroupType::Compute)].push_back(engine);
    }
    if (engine.getEngineType() == aub_stream::ENGINE_BCS && DebugManager.flags.EnableBlitterOperationsSupport.get() != 0) {
        engineGroups[static_cast<uint32_t>(EngineGroupType::Copy)].push_back(engine);
    }
}

template <>
bool HwHelperHw<Family>::forceBlitterUseForGlobalBuffers(const HardwareInfo &hwInfo, GraphicsAllocation *allocation) const {
    return Gen12LPHelpers::forceBlitterUseForGlobalBuffers(hwInfo, allocation);
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    if (Gen12LPHelpers::pipeControlWaRequired(hwInfo.platform.eProductFamily)) {
        HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo)) {
            PIPE_CONTROL cmd = Family::cmdInitPipeControl;
            cmd.setCommandStreamerStallEnable(true);
            auto pipeControl = static_cast<Family::PIPE_CONTROL *>(commandStream.getSpace(sizeof(PIPE_CONTROL)));
            *pipeControl = cmd;
        }
    }
}

template <>
std::string HwHelperHw<Family>::getExtensions() const {
    return "cl_intel_subgroup_local_block_io ";
}

template <>
inline void MemorySynchronizationCommands<Family>::setPipeControlExtraProperties(PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);
}

template <>
void MemorySynchronizationCommands<Family>::setCacheFlushExtraProperties(Family::PIPE_CONTROL &pipeControl) {
    pipeControl.setHdcPipelineFlush(true);
    pipeControl.setConstantCacheInvalidationEnable(false);
}

template <>
bool HwHelperHw<Family>::useOnlyGlobalTimestamps() const {
    return true;
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
