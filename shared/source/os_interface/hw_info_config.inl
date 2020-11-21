/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableRenderCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSharedSystemMemCapabilities() {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isEvenContextCountRequired() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::enableRenderCompression(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.ftrE2ECompression;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::enableBlitterOperationsSupport(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.blitterOperationsSupported = HwHelper::get(hwInfo->platform.eRenderCoreFamily).obtainBlitterPreference(*hwInfo);

    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hwInfo->capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}
} // namespace NEO
