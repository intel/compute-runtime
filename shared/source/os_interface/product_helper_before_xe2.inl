/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
bool ProductHelperHw<gfxProduct>::isPidFdOrSocketForIpcSupported() const {
    if (debugManager.flags.EnablePidFdOrSocketsForIpc.get() != -1) {
        return debugManager.flags.EnablePidFdOrSocketsForIpc.get();
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const {
}

} // namespace NEO
