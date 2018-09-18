/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/hw_info_config.inl"

namespace OCLRT {

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

} // namespace OCLRT
