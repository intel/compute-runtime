/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

#include "aubstream/product_family.h"

namespace NEO {
template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename XeHpgCoreFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }

    auto isMirrorAddressMode = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR == samplerState->getTcxAddressControlMode();
    auto isNearestFilter = SAMPLER_STATE::MIN_MODE_FILTER_NEAREST == samplerState->getMinModeFilter();
    auto isWorkaroundRequired = (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo, *this)) ||
                                (DG2::isG11(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this));
    if (isNearestFilter && isMirrorAddressMode && isWorkaroundRequired) {
        samplerState->setRAddressMinFilterRoundingEnable(true);
        samplerState->setRAddressMagFilterRoundingEnable(true);
    }
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
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
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
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
bool ProductHelperHw<gfxProduct>::isGlobalFenceInDirectSubmissionRequired(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hwInfo, *this)) {
        return true;
    }
    if (DG2::isG11(hwInfo)) {
        return true;
    }
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hwInfo, *this);
}

template <>
void ProductHelperHw<gfxProduct>::setForceNonCoherent(void *const statePtr, const StateComputeModeProperties &properties) const {
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
bool ProductHelperHw<gfxProduct>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this)) {
        return false;
    }
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hwInfo, *this)) {
        return false;
    }
    return true;
}

template <>
LocalMemoryAccessMode ProductHelperHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this)) {
        return LocalMemoryAccessMode::CpuAccessDisallowed;
    }
    return LocalMemoryAccessMode::Default;
}

template <>
bool ProductHelperHw<gfxProduct>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
bool ProductHelperHw<gfxProduct>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isBasicWARequired = true;
    auto isExtendedWARequired = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1 && !isRcs;

    if (DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool ProductHelperHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo, *this)) {
        return false;
    }
    if (DG2::isG11(hwInfo) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this)) {
        return false;
    }
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    const uint32_t highestEnabledSlice = NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo);

    auto subSlicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    auto maxSubSlice = highestEnabledSlice * subSlicesPerSlice;

    if (highestEnabledSlice == 0) {
        UNRECOVERABLE_IF(true);
    }
    return maxSubSlice;
}

template <>
bool ProductHelperHw<gfxProduct>::isFlushTaskAllowed() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isCpuCopyNecessary(const void *ptr, MemoryManager *memoryManager) const {
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
bool ProductHelperHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    if constexpr (is32bit) {
        return true;
    } else {
        return false;
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ) const {
    const bool enabled = !isOOQ;
    if (DebugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return DebugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Dg2;
};

} // namespace NEO
