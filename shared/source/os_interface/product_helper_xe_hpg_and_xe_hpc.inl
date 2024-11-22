/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
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
    return (UnifiedSharedMemoryFlags::access);
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
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.flags.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.flags.ftrE2ECompression;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    if (isMaxThreadsForWorkgroupWARequired(hwInfo)) {
        return std::min(getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice), 64u);
    }
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useGemCreateExtInAllocateMemoryByKMD() const {
    return false;
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
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
    coherencyFlag = false;
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
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getInternalHeapsPreallocated() const {
    if (debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get() != -1) {
        return debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get();
    }
    return 1u;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 2u;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::useLocalPreferredForCacheableBuffers() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return true;
}

} // namespace NEO
