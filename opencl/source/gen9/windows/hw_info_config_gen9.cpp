/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_plus.inl"

namespace NEO {

#ifdef SUPPORT_BXT
template <>
int HwInfoConfigHw<IGFX_BROXTON>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_BROXTON>;
#endif
#ifdef SUPPORT_CFL
template <>
int HwInfoConfigHw<IGFX_COFFEELAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_COFFEELAKE>;
#endif
#ifdef SUPPORT_GLK
template <>
int HwInfoConfigHw<IGFX_GEMINILAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_GEMINILAKE>;
#endif
#ifdef SUPPORT_KBL
template <>
int HwInfoConfigHw<IGFX_KABYLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_KABYLAKE>;
#endif
#ifdef SUPPORT_SKL
template <>
int HwInfoConfigHw<IGFX_SKYLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_SKYLAKE>;
#endif

} // namespace NEO
