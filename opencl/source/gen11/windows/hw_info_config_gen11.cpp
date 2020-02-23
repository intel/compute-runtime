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

#ifdef SUPPORT_ICLLP
template <>
int HwInfoConfigHw<IGFX_ICELAKE_LP>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_ICELAKE_LP>;
#endif
#ifdef SUPPORT_LKF
template <>
int HwInfoConfigHw<IGFX_LAKEFIELD>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_LAKEFIELD>;
#endif

#ifdef SUPPORT_EHL
template <>
int HwInfoConfigHw<IGFX_ELKHARTLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_ELKHARTLAKE>;
#endif
} // namespace NEO
