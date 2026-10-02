/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useGemCreateExtInAllocateMemoryByKMD() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::deferMOCSToPatIndex(bool isWddmOnLinux) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCopyBufferRectSplitSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    return std::nullopt;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setRenderCompressedFlags(HardwareInfo &hwInfo) const {
    hwInfo.capabilityTable.ftrRenderCompressedImages = hwInfo.featureTable.flags.ftrE2ECompression;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = hwInfo.featureTable.flags.ftrE2ECompression;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCompressionForbidden(const HardwareInfo &hwInfo) const {
    return isCompressionForbiddenCommon(true);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isResourceUncachedForCS(AllocationType allocationType) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isNonCoherentTimestampsModeEnabled() const {
    if (debugManager.flags.ForceNonCoherentModeForTimestamps.get() != -1) {
        return debugManager.flags.ForceNonCoherentModeForTimestamps.get();
    }
    return !this->isDcFlushAllowed();
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const {
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isVmBindDecompressionProbeAllowed(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getIsaPrefetchSize(uint32_t isaSize) const {
    constexpr size_t defaultLimitValue = MemoryConstants::kiloByte;

    uint32_t limitValue = debugManager.flags.LimitIsaPrefetchSize.getIfNotDefault(static_cast<uint32_t>(defaultLimitValue));
    return std::min(isaSize, limitValue);
}

} // namespace NEO
