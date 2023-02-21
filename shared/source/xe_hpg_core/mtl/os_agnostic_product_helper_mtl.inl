/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_type.h"

#include "aubstream/product_family.h"

namespace NEO {
template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename XeHpgCoreFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x1;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    switch (hwInfo.ipVersion.release) {
    case 70:
        switch (hwInfo.ipVersion.revision) {
        case 0:
            return AOT::XE_LPG_MD_A0;
        case 4:
            return AOT::XE_LPG_MD_B0;
        default:
            return AOT::UNKNOWN_ISA;
        }
    case 71:
        switch (hwInfo.ipVersion.revision) {
        case 0:
            return AOT::XE_LPG_LG_A0;
        case 4:
            return AOT::XE_LPG_LG_B0;
        default:
            return AOT::UNKNOWN_ISA;
        }
    default:
        return AOT::UNKNOWN_ISA;
    }
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_B;
    }
    return CommonConstants::invalidStepping;
}

template <>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isBasicWARequired = GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
    auto isExtendedWARequired = false;

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool ProductHelperHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const {
    return (MTL::isLpg(hwInfo) == false);
}

template <>
bool ProductHelperHw<gfxProduct>::isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const {
    return (MTL::isLpg(hwInfo) == false);
}

template <>
bool ProductHelperHw<gfxProduct>::isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const {
    return (MTL::isLpg(hwInfo) == false);
}

template <>
bool ProductHelperHw<gfxProduct>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isEvictionIfNecessaryFlagSupported() const {
    return true;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Mtl;
};

template <>
uint64_t ProductHelperHw<gfxProduct>::overridePatIndex(AllocationType allocationType, uint64_t patIndex) const {
    if (allocationType == AllocationType::TIMESTAMP_PACKET_TAG_BUFFER) {
        constexpr uint64_t uncached = 2u;
        return uncached;
    }
    return patIndex;
}

template <>
int ProductHelperHw<gfxProduct>::getProductMaxPreferredSlmSize(const HardwareInfo &hwInfo, int preferredEnumValue) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    if (getProductConfigFromHwInfo(hwInfo) == AOT::XE_LPG_MD_A0 || getProductConfigFromHwInfo(hwInfo) == AOT::XE_LPG_LG_A0) {
        return static_cast<int>(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
    } else {
        return preferredEnumValue;
    }
}
} // namespace NEO
