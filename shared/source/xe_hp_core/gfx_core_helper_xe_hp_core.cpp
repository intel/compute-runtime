/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/aub_mapper.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

using Family = NEO::XeHpFamily;

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/extra_allocation_data_xehp_and_later.inl"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_bdw_to_dg2.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"
#include "shared/source/helpers/logical_state_helper.inl"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
template <>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Blit;

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHP);
}

template <>
uint32_t GfxCoreHelperHw<Family>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (DebugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    // XeHP products return physical threads
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    return std::max(hwInfo->gtSystemInfo.MaxSubSlicesSupported, static_cast<uint32_t>(32)) * hwInfo->gtSystemInfo.MaxEuPerSubSlice * (hwInfo->gtSystemInfo.ThreadCount / hwInfo->gtSystemInfo.EUCount);
}

template <>
uint32_t GfxCoreHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) {
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
void GfxCoreHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const {
}

template <>
bool GfxCoreHelperHw<Family>::isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {

    bool forceOverrideMemoryBankIndex = (GfxCoreHelper::getSubDevicesCount(&hwInfo) == 4 &&
                                         GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, productHelper));

    if (DebugManager.flags.ForceMemoryBankIndexOverride.get() != -1) {
        forceOverrideMemoryBankIndex = static_cast<bool>(DebugManager.flags.ForceMemoryBankIndexOverride.get());
    }
    return forceOverrideMemoryBankIndex;
}

template <>
bool GfxCoreHelperHw<Family>::isSipWANeeded(const HardwareInfo &hwInfo) const {
    return hwInfo.platform.usRevId <= ProductHelper::get(hwInfo.platform.eProductFamily)->getHwRevIdFromStepping(REVISION_B, hwInfo);
}

template <>
bool GfxCoreHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!DebugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    if (ProductHelper::get(hwInfo.platform.eProductFamily)->allowStatelessCompression(hwInfo)) {
        return true;
    } else {
        return size > KB;
    }
}

template <>
const StackVec<uint32_t, 6> GfxCoreHelperHw<Family>::getThreadsPerEUConfigs() const {
    return {4, 8};
}

template <>
std::string GfxCoreHelperHw<Family>::getExtensions(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    std::string extensions;
    extensions += "cl_intel_dot_accumulate ";
    extensions += "cl_intel_subgroup_local_block_io ";
    extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";

    return extensions;
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierWaFlags(void *barrierCmd) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setCommandStreamerStallEnable(true);
    pipeControl.setHdcPipelineFlush(true);
}

template <>
void MemorySynchronizationCommands<Family>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename Family::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);

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
    if (hwInfo.featureTable.flags.ftrLocalMemory) {
        args.hdcPipelineFlush = true;
    }
}

template <>
void MemorySynchronizationCommands<Family>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
}

template <>
bool GfxCoreHelperHw<Family>::unTypedDataPortCacheFlushRequired() const {
    return false;
}

template <>
bool GfxCoreHelperHw<Family>::disableL3CacheForDebug(const HardwareInfo &) const {
    return true;
}
template <>
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    if (grfCount > GrfConfig::DefaultGrfNumber) {
        return hwInfo.gtSystemInfo.ThreadCount / 2u;
    }
    return hwInfo.gtSystemInfo.ThreadCount;
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;

template LogicalStateHelper *LogicalStateHelper::create<Family>();
} // namespace NEO
