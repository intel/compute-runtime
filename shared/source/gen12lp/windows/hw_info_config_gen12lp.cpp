/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {
namespace Gen12LPCommonFunctions {
inline void adjustPlatformForProductFamily(PLATFORM &platform, GFXCORE_FAMILY newCoreFamily) {
    platform.eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform.eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

} // namespace Gen12LPCommonFunctions

#ifdef SUPPORT_TGLLP

template <>
void HwInfoConfigHw<IGFX_TIGERLAKE_LP>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}

template class HwInfoConfigHw<IGFX_TIGERLAKE_LP>;
#endif
#ifdef SUPPORT_DG1

template <>
void HwInfoConfigHw<IGFX_DG1>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}

template class HwInfoConfigHw<IGFX_DG1>;
#endif
#ifdef SUPPORT_RKL

template <>
void HwInfoConfigHw<IGFX_ROCKETLAKE>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}

template class HwInfoConfigHw<IGFX_ROCKETLAKE>;
#endif
#ifdef SUPPORT_ADLS

template <>
void HwInfoConfigHw<IGFX_ALDERLAKE_S>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}

template class HwInfoConfigHw<IGFX_ALDERLAKE_S>;
#endif
} // namespace NEO
