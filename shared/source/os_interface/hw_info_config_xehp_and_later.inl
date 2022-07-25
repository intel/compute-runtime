/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesValue() {
    return (UNIFIED_SHARED_MEMORY_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getCrossDeviceSharedMemCapabilities() {
    uint64_t capabilities = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS;

    if (getConcurrentAccessMemCapabilitiesSupported(UsmAccessCapabilities::SharedCrossDevice)) {
        capabilities |= UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
    }

    return capabilities;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::enableCompression(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.flags.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.flags.ftrE2ECompression;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    if (isMaxThreadsForWorkgroupWARequired(hwInfo)) {
        return std::min(getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice), 64u);
    }
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBlitterFullySupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.blitterOperationsSupported;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isNewResidencyModelSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) {
    coherencyFlag = false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isEvictionWhenNecessaryFlagSupported() const {
    return true;
}

} // namespace NEO
