/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useGemCreateExtInAllocateMemoryByKMD() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::deferMOCSToPatIndex(bool isWddmOnLinux) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCopyBufferRectSplitSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    std::array<uint64_t, 11> listOfCoherentPatIndexes = {1, 2, 4, 5, 7, 22, 23, 26, 27, 30, 31};
    if (std::find(listOfCoherentPatIndexes.begin(), listOfCoherentPatIndexes.end(), patIndex) != listOfCoherentPatIndexes.end()) {
        return true;
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setRenderCompressedFlags(HardwareInfo &hwInfo) const {
    hwInfo.capabilityTable.ftrRenderCompressedImages = hwInfo.featureTable.flags.ftrXe2Compression;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = hwInfo.featureTable.flags.ftrXe2Compression;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isCompressionForbidden(const HardwareInfo &hwInfo) const {
    return isCompressionForbiddenCommon(false);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isResourceUncachedForCS(AllocationType allocationType) const {
    return GraphicsAllocation::isAccessedFromCommandStreamer(allocationType);
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
    timeoutUs = 1'000;
    maxTimeoutUs = 1'000;
    if (debugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeoutUs = debugManager.flags.DirectSubmissionControllerTimeout.get();
    }
    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeoutUs = debugManager.flags.DirectSubmissionControllerMaxTimeout.get();
    }
}

} // namespace NEO
