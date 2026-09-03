/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include "aubstream/product_family.h"
#include "neo_aot_platforms.h"

#include <algorithm>
#include <array>

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isPatIndexValidForUserptr(uint64_t patIndex) const {
    constexpr std::array<uint64_t, 6> validPatIndices = {2, 7, 19, 23, 27, 31};
    return std::find(validPatIndices.begin(), validPatIndices.end(), patIndex) != validPatIndices.end();
}

template <>
bool ProductHelperHw<gfxProduct>::overrideAllocationCpuCacheable(const AllocationData &allocationData) const {
    return GraphicsAllocation::isAccessedFromCommandStreamer(allocationData.type);
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Nvlp;
};

template <>
bool ProductHelperHw<gfxProduct>::isAvailableExtendedScratch() const {
    return debugManager.flags.EnableExtendedScratchSurfaceSize.get();
}

template <>
bool ProductHelperHw<gfxProduct>::isL3FlushAfterPostSyncSupported() const {

    if (debugManager.flags.EnableL3FlushAfterPostSync.get() != -1) {
        return debugManager.flags.EnableL3FlushAfterPostSync.get() == 1;
    }
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForQueues() const {

    if (debugManager.flags.EnableL3FlushAfterPostSync.get() != -1) {
        return debugManager.flags.EnableL3FlushAfterPostSync.get() == 1;
    }

    return true;
}

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::adjustMaxThreadsPerThreadGroup(const HardwareInfo &hwInfo, uint32_t maxThreadsPerThreadGroup, uint32_t simt, uint32_t grfCount) const {
    auto adjustedMaxThreadsPerThreadGroup = maxThreadsPerThreadGroup;

    if (hwInfo.ipVersion.value == AOT::NVL_P_A0) {
        return adjustedMaxThreadsPerThreadGroup;
    }

    if (grfCount == 448) {
        adjustedMaxThreadsPerThreadGroup = 16u;
    } else if (grfCount == 320) {
        adjustedMaxThreadsPerThreadGroup = 24u;
    }

    return adjustedMaxThreadsPerThreadGroup;
}

template <>
std::optional<uint8_t> ProductHelperHw<gfxProduct>::getBcsCompressionFormat() const {
    return uint8_t{0x2};
}

} // namespace NEO
