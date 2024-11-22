/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/unified_memory/usm_memory_support.h"

#include <algorithm>

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
const std::vector<uint32_t> ProductHelperHw<gfxProduct>::getSupportedLocalDispatchSizes(const HardwareInfo &hwInfo) const {
    return {};
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getMaxLocalRegionSize(const HardwareInfo &hwInfo) const {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getCrossDeviceSharedMemCapabilities() const {
    uint64_t capabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::sharedCrossDevice)) {
        capabilities |= UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::enableCompression(HardwareInfo *hwInfo) const {
    const bool isCompressionEnabled = hwInfo->featureTable.flags.ftrE2ECompression && hwInfo->featureTable.flags.ftrXe2Compression;
    hwInfo->capabilityTable.ftrRenderCompressedImages = isCompressionEnabled;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = isCompressionEnabled;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useGemCreateExtInAllocateMemoryByKMD() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitterFullySupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.blitterOperationsSupported;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isNewResidencyModelSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::deferMOCSToPatIndex() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::heapInLocalMem(const HardwareInfo &hwInfo) const {
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
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 2u;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getInternalHeapsPreallocated() const {
    if (debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get() != -1) {
        return debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get();
    }
    return 1u;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
    coherencyFlag = false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isUsmPoolAllocatorSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useLocalPreferredForCacheableBuffers() const {
    return false;
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
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isHostUsmAllocationReuseSupported() const {
    return true;
}

} // namespace NEO
