/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {
namespace Gen12LPCommonFunctions {
inline void adjustPlatformForProductFamily(PLATFORM &platform, GFXCORE_FAMILY newCoreFamily) {
    platform.eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform.eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

} // namespace Gen12LPCommonFunctions

} // namespace NEO

#ifdef SUPPORT_TGLLP
namespace NEO {
template <>
void HwInfoConfigHw<IGFX_TIGERLAKE_LP>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}
} // namespace NEO

#include "hw_info_config_tgllp.inl"
#endif
#ifdef SUPPORT_DG1
namespace NEO {
template <>
void HwInfoConfigHw<IGFX_DG1>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}
} // namespace NEO

#include "hw_info_config_dg1.inl"
#endif
#ifdef SUPPORT_RKL
namespace NEO {
template <>
void HwInfoConfigHw<IGFX_ROCKETLAKE>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}
} // namespace NEO

#include "hw_info_config_rkl.inl"
#endif
#ifdef SUPPORT_ADLS
namespace NEO {
template <>
void HwInfoConfigHw<IGFX_ALDERLAKE_S>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    Gen12LPCommonFunctions::adjustPlatformForProductFamily(hwInfo->platform, GFXCORE_FAMILY::IGFX_GEN12LP_CORE);
}
} // namespace NEO

#include "hw_info_config_adls.inl"
#endif