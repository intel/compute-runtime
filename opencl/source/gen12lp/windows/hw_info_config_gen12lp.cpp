/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_info_config_common_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {

#ifdef SUPPORT_TGLLP
template <>
int HwInfoConfigHw<IGFX_TIGERLAKE_LP>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.ftrE2ECompression;

    return 0;
}

template <>
void HwInfoConfigHw<IGFX_TIGERLAKE_LP>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    platform->eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform->eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

template class HwInfoConfigHw<IGFX_TIGERLAKE_LP>;
#endif
#ifdef SUPPORT_DG1
template <>
int HwInfoConfigHw<IGFX_DG1>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.ftrE2ECompression;
    HwInfoConfigCommonHelper::enableBlitterOperationsSupport(*hwInfo);
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_DG1>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    platform->eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform->eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

template <>
uint64_t HwInfoConfigHw<IGFX_DG1>::getSharedSystemMemCapabilities() {
    return 0;
}

template class HwInfoConfigHw<IGFX_DG1>;
#endif
#ifdef SUPPORT_RKL
template <>
int HwInfoConfigHw<IGFX_ROCKETLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.ftrE2ECompression;
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_ROCKETLAKE>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    platform->eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform->eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

template class HwInfoConfigHw<IGFX_ROCKETLAKE>;
#endif

} // namespace NEO
