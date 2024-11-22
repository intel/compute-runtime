/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
void ProductHelperHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
    hwInfo->platform.eDisplayCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
}

template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}

template <>
const std::vector<uint32_t> ProductHelperHw<gfxProduct>::getSupportedLocalDispatchSizes(const HardwareInfo &hwInfo) const {
    return {};
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxLocalRegionSize(const HardwareInfo &hwInfo) const {
    return 0;
}
} // namespace NEO
