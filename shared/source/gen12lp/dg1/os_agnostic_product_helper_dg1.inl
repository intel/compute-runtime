/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"

#include "aubstream/product_family.h"

namespace NEO {

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
bool ProductHelperHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return UnifiedSharedMemoryFlags::access;
}

template <>
bool ProductHelperHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
bool ProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo, *this);
}

template <>
bool ProductHelperHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::overrideGfxPartitionLayoutForWsl() const {
    return true;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Dg1;
};

} // namespace NEO
