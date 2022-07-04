/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/aub_mapper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

using Family = NEO::XE_HPG_COREFamily;

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/extra_allocation_data_xehp_and_later.inl"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_dg2_and_later.inl"
#include "shared/source/helpers/hw_helper_tgllp_and_later.inl"
#include "shared/source/helpers/hw_helper_xehp_and_later.inl"
#include "shared/source/helpers/logical_state_helper.inl"

namespace NEO {
template <>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Blit;

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
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHPG);
}

template <>
void HwHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
    if (HwInfoConfig::get(pHwInfo->platform.eProductFamily)->isDefaultEngineTypeAdjustmentRequired(*pHwInfo)) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
}

template <>
bool HwHelperHw<Family>::is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const {
    return !hwInfo.workaroundTable.flags.waAuxTable64KGranular && isCompressionEnabled;
}

template <>
void HwHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {
    if (useL1Cache) {
        surfaceState->setL1CachePolicyL1CacheControl(Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB);
        if (DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get() != -1) {
            surfaceState->setL1CachePolicyL1CacheControl(static_cast<typename Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY>(DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get()));
        }
    }
}

template <>
bool HwHelperHw<Family>::isBankOverrideRequired(const HardwareInfo &hwInfo) const {

    bool forceOverrideMemoryBankIndex = false;

    if (DebugManager.flags.ForceMemoryBankIndexOverride.get() != -1) {
        forceOverrideMemoryBankIndex = static_cast<bool>(DebugManager.flags.ForceMemoryBankIndexOverride.get());
    }
    return forceOverrideMemoryBankIndex;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(const HardwareInfo &hwInfo) {
    return EncodeSempahore<Family>::getSizeMiSemaphoreWait();
}

template <>
void MemorySynchronizationCommands<Family>::addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo) {
    using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;

    EncodeSempahore<Family>::addMiSemaphoreWaitCommand(commandStream, gpuAddress, EncodeSempahore<Family>::invalidHardwareTag, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
}

template <>
const StackVec<uint32_t, 6> HwHelperHw<Family>::getThreadsPerEUConfigs() const {
    return {4, 8};
}

template <>
std::string HwHelperHw<Family>::getExtensions(const HardwareInfo &hwInfo) const {
    std::string extensions;
    extensions += "cl_intel_create_buffer_with_properties ";
    extensions += "cl_intel_dot_accumulate ";
    extensions += "cl_intel_subgroup_local_block_io ";

    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isMatrixMultiplyAccumulateSupported(hwInfo)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
        extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";
    }

    return extensions;
}

template <>
bool HwHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!DebugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }

    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->allowStatelessCompression(hwInfo)) {
        return true;
    } else {
        return false;
    }
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
bool HwHelperHw<Family>::disableL3CacheForDebug(const HardwareInfo &hwInfo) const {
    return isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwHelperHw<Family>::isTimestampWaitSupportedForEvents(const HardwareInfo &hwInfo) const {
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    return hwInfoConfig.isTimestampWaitSupportedForEvents();
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;

template LogicalStateHelper *LogicalStateHelper::create<Family>();
} // namespace NEO
