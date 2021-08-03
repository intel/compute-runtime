/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/aub_mapper.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

using Family = NEO::XeHpFamily;

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/extra_allocation_data_xehp_plus.inl"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_tgllp_plus.inl"
#include "shared/source/helpers/hw_helper_xehp_plus.inl"

namespace NEO {
template <>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Blit;

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHP);
}

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    if (DebugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    // XeHP plus products return physical threads
    return std::max(pHwInfo->gtSystemInfo.MaxSubSlicesSupported, static_cast<uint32_t>(32)) * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * (pHwInfo->gtSystemInfo.ThreadCount / pHwInfo->gtSystemInfo.EUCount);
}

template <>
inline bool HwHelperHw<Family>::isSpecialWorkgroupSizeRequired(const HardwareInfo &hwInfo, bool isSimulation) const {
    if (DebugManager.flags.ForceWorkgroupSize1x1x1.get() != -1) {
        return static_cast<bool>(DebugManager.flags.ForceWorkgroupSize1x1x1.get());
    } else {
        HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        return (!isSimulation && hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo) && hwHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed);
    }
}

template <>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwHelperHw<Family>::isNewResidencyModelSupported() const {
    return true;
}

template <>
uint32_t HwHelperHw<Family>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_XE_HP_SDV) {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_A1:
            return 0x1;
        case REVISION_B:
            return 0x4;
        }
    }
    return CommonConstants::invalidStepping;
}

template <>
bool HwHelperHw<Family>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.usRevId < getHwRevIdFromStepping(REVISION_B, hwInfo)) {
        return false;
    }
    return true;
}

template <>
uint32_t HwHelperHw<Family>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_XE_HP_SDV) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return REVISION_A0;
        case 0x1:
            return REVISION_A1;
        case 0x4:
            return REVISION_B;
        }
    }
    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    auto slmValue = std::max(slmSize, 1024u);
    slmValue = Math::nextPowerOfTwo(slmValue);
    slmValue = Math::getMinLsbSet(slmValue);
    slmValue = slmValue - 9;
    DEBUG_BREAK_IF(slmValue > 7);
    slmValue *= !!slmSize;

    return slmValue;
}

template <>
void HwHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {
}

template <>
inline bool HwHelperHw<Family>::allowRenderCompression(const HardwareInfo &hwInfo) const {
    if (hwInfo.gtSystemInfo.EUCount == 256u) {
        return false;
    }

    return true;
}

template <>
inline bool HwHelperHw<Family>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    if (!NEO::ApiSpecificConfig::isStatelessCompressionSupported()) {
        return false;
    }
    if (DebugManager.flags.EnableStatelessCompression.get() != -1) {
        return static_cast<bool>(DebugManager.flags.EnableStatelessCompression.get());
    }
    if (HwHelper::getSubDevicesCount(&hwInfo) > 1) {
        return DebugManager.flags.EnableMultiTileCompression.get() > 0 ? true : false;
    }
    if (hwInfo.platform.usRevId < getHwRevIdFromStepping(REVISION_B, hwInfo)) {
        return false;
    }
    return true;
}

template <>
bool HwHelperHw<Family>::isBankOverrideRequired(const HardwareInfo &hwInfo) const {

    bool forceOverrideMemoryBankIndex = (HwHelper::getSubDevicesCount(&hwInfo) == 4 &&
                                         isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo));

    if (DebugManager.flags.ForceMemoryBankIndexOverride.get() != -1) {
        forceOverrideMemoryBankIndex = static_cast<bool>(DebugManager.flags.ForceMemoryBankIndexOverride.get());
    }
    return forceOverrideMemoryBankIndex;
}

template <>
bool HwHelperHw<Family>::isSipWANeeded(const HardwareInfo &hwInfo) const {
    return hwInfo.platform.usRevId <= getHwRevIdFromStepping(REVISION_B, hwInfo);
}

template <>
bool HwHelperHw<Family>::isBufferSizeSuitableForRenderCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (allowStatelessCompression(hwInfo)) {
        return true;
    } else {
        return size > KB;
    }
}

template <>
LocalMemoryAccessMode HwHelperHw<Family>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    if (isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo)) {
        return LocalMemoryAccessMode::CpuAccessDisallowed;
    }
    return LocalMemoryAccessMode::Default;
}

template <>
const StackVec<uint32_t, 6> HwHelperHw<Family>::getThreadsPerEUConfigs() const {
    return {4, 8};
}

template <>
std::string HwHelperHw<Family>::getExtensions() const {
    std::string extensions;
    extensions += "cl_intel_dot_accumulate ";
    extensions += "cl_intel_subgroup_local_block_io ";

    return extensions;
}

template <>
bool HwHelperHw<Family>::isBlitterForImagesSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool HwHelperHw<Family>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return (this->getSteppingFromHwRevId(hwInfo) >= REVISION_B);
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;

    if (DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        if (hwInfo.featureTable.ftrLocalMemory) {
            PIPE_CONTROL cmd = Family::cmdInitPipeControl;
            cmd.setCommandStreamerStallEnable(true);
            cmd.setHdcPipelineFlush(true);

            auto pipeControl = static_cast<PIPE_CONTROL *>(commandStream.getSpace(sizeof(PIPE_CONTROL)));
            *pipeControl = cmd;
        }
    }
}

template <>
void MemorySynchronizationCommands<Family>::setPipeControlExtraProperties(PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pipeControl.setHdcPipelineFlush(true);
        pipeControl.setCompressionControlSurfaceCcsFlush(true);
    }
    if (DebugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setHdcPipelineFlush(false);
        pipeControl.setCompressionControlSurfaceCcsFlush(false);
    }
}

template <>
void MemorySynchronizationCommands<Family>::setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo) {
    if (hwInfo.featureTable.ftrLocalMemory) {
        args.hdcPipelineFlush = true;
    }
}

template <>
void MemorySynchronizationCommands<Family>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
}

template <>
bool HwHelperHw<Family>::additionalPipeControlArgsRequired() const {
    return false;
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
