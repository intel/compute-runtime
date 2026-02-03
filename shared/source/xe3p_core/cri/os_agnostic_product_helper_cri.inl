/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/simd_helper.h"

#include "aubstream/engine_node.h"
#include "aubstream/product_family.h"

#include <algorithm>

namespace NEO {

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Cri;
}

template <>
bool ProductHelperHw<gfxProduct>::isAvailableExtendedScratch() const {
    return debugManager.flags.EnableExtendedScratchSurfaceSize.get();
}

template <>
bool ProductHelperHw<gfxProduct>::isLinearStoragePreferred(bool isImage1d, bool forceLinearStorage) const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxNumSamplers() const {
    return 0u;
}

template <>
aub_stream::EngineType ProductHelperHw<gfxProduct>::getDefaultCopyEngine() const {
    return aub_stream::EngineType::ENGINE_BCS1;
}

template <>
void ProductHelperHw<gfxProduct>::adjustEngineGroupType(EngineGroupType &engineGroupType) const {
    if (engineGroupType == EngineGroupType::linkedCopy) {
        engineGroupType = EngineGroupType::copy;
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isImplicitScalingSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceToHostCopySignalingFenceRequired() const {
    return true;
}

template <>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    std::array<uint64_t, 7> listOfNonCoherentPatIndexes = {0, 3, 5, 8, 23, 26, 29};
    if (std::find(listOfNonCoherentPatIndexes.begin(), listOfNonCoherentPatIndexes.end(), patIndex) != listOfNonCoherentPatIndexes.end()) {
        return false;
    }
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getNumCacheRegions() const {
    return static_cast<uint32_t>(CacheRegion::count);
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const {
    uint64_t patIndex{0u};
    switch (cacheRegion) {
    case CacheRegion::region1:
        patIndex = 23u;
        break;
    case CacheRegion::region2:
        patIndex = 24u;
        break;
    default:
        break;
    }

    return patIndex;
}

template <>
bool ProductHelperHw<gfxProduct>::areSecondaryContextsSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::is2MBLocalMemAlignmentEnabled() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isSharingWith3dOrMediaAllowed() const {
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::adjustMaxThreadsPerThreadGroup(uint32_t maxThreadsPerThreadGroup, uint32_t simt, uint32_t grfCount, bool isHeaplessModeEnabled) const {
    auto adjustedMaxThreadsPerThreadGroup = maxThreadsPerThreadGroup;
    if (grfCount == 512) {
        adjustedMaxThreadsPerThreadGroup = 32u;
    } else if (isHeaplessModeEnabled && isSimd1(simt) && (grfCount == 160 || grfCount == 192)) {
        adjustedMaxThreadsPerThreadGroup = 64u;
    }
    return adjustedMaxThreadsPerThreadGroup;
}

template <>
bool ProductHelperHw<gfxProduct>::shouldRegisterEnqueuedWalkerWithProfiling() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isInterruptSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isMediaContextSupported() const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getPreferredWorkgroupCountPerSubslice() const {
    return 4;
}

} // namespace NEO
