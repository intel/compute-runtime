/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/hw_info_config.inl"
#include "runtime/os_interface/hw_info_config_bdw_plus.inl"

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

} // namespace NEO
