/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_type.h"

#include "aubstream/product_family.h"
#include "platforms.h"

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
    return CommonConstants::invalidStepping;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs, const ReleaseHelper *releaseHelper) const {
    auto isBasicWARequired = true;
    if (releaseHelper) {
        isBasicWARequired = releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired();
    }
    auto isExtendedWARequired = false;

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool ProductHelperHw<gfxProduct>::programAllStateComputeCommandFields() const {
    return true;
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
    if (hwInfo.ipVersion.value == AOT::MTL_M_A0 || hwInfo.ipVersion.value == AOT::MTL_P_A0) {
        return static_cast<int>(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
    } else {
        return preferredEnumValue;
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isDummyBlitWaRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isCachingOnCpuAvailable() const {
    return false;
}

} // namespace NEO
