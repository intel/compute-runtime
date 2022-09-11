/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

template <>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename XeHpgCoreFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }

    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto isMirrorAddressMode = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR == samplerState->getTcxAddressControlMode();
    auto isNearestFilter = SAMPLER_STATE::MIN_MODE_FILTER_NEAREST == samplerState->getMinModeFilter();
    if (isNearestFilter && isMirrorAddressMode && hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo)) {
        samplerState->setRAddressMinFilterRoundingEnable(true);
        samplerState->setRAddressMagFilterRoundingEnable(true);
    }
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_A1:
        return 0x1;
    case REVISION_B:
        return 0x4;
    case REVISION_C:
        return 0x8;
    default:
        return CommonConstants::invalidStepping;
    }
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_A1;
    case 0x4:
        return REVISION_B;
    case 0x8:
        return REVISION_C;
    default:
        return CommonConstants::invalidStepping;
    }
}

template <>
bool HwInfoConfigHw<gfxProduct>::isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    uint32_t stepping = getSteppingFromHwRevId(hwInfo);
    if (stepping <= REVISION_B) {
        return true;
    } else {
        return false;
    }
}

template <>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    uint32_t stepping = getSteppingFromHwRevId(hwInfo);
    return (REVISION_A0 == stepping && DG2::isG10(hwInfo));
}

template <>
void HwInfoConfigHw<gfxProduct>::setForceNonCoherent(void *const statePtr, const StateComputeModeProperties &properties) {
    using STATE_COMPUTE_MODE = typename XeHpgCoreFamily::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE &stateComputeMode = *static_cast<STATE_COMPUTE_MODE *>(statePtr);
    FORCE_NON_COHERENT coherencyValue = (properties.isCoherencyRequired.value == 1) ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED
                                                                                    : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    stateComputeMode.setForceNonCoherent(coherencyValue);

    auto mask = stateComputeMode.getMaskBits();
    mask |= XeHpgCoreFamily::stateComputeModeForceNonCoherentMask;
    stateComputeMode.setMaskBits(mask);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B || DG2::isG11(hwInfo) || DG2::isG12(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    if (HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_A1, hwInfo) &&
        (hwInfo.gtSystemInfo.EUCount != 128)) {
        return false;
    }
    return true;
}

template <>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    if (HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo)) {
        return LocalMemoryAccessMode::CpuAccessDisallowed;
    }
    return LocalMemoryAccessMode::Default;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
std::pair<bool, bool> HwInfoConfigHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isBasicWARequired = true;
    auto isExtendedWARequired = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1 && !isRcs;

    if (DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool HwInfoConfigHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_C;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    auto highestEnabledSlice = 0;
    for (int highestSlice = GT_MAX_SLICE - 1; highestSlice >= 0; highestSlice--) {
        if (hwInfo.gtSystemInfo.SliceInfo[highestSlice].Enabled) {
            highestEnabledSlice = highestSlice + 1;
            break;
        }
    }

    auto subSlicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    auto maxSubSlice = highestEnabledSlice * subSlicesPerSlice;

    if (highestEnabledSlice == 0) {
        UNRECOVERABLE_IF(true);
    }
    return maxSubSlice;
}

template <>
void HwInfoConfigHw<gfxProduct>::convertTimestampsFromOaToCsDomain(uint64_t &timestampData) {
    timestampData >>= 1;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isFlushTaskAllowed() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const {
    if (memoryManager) {
        if constexpr (is32bit) {
            return memoryManager->isWCMemory(ptr);
        } else {
            return false;
        }
    } else {
        return false;
    }
}
template <>
bool HwInfoConfigHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    if constexpr (is32bit) {
        return true;
    } else {
        return false;
    }
}
