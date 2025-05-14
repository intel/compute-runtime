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
    return true;
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

} // namespace NEO
