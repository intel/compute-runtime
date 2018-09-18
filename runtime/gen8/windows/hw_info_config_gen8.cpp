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

#ifdef SUPPORT_BDW
template <>
int HwInfoConfigHw<IGFX_BROADWELL>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    return 0;
}

template class HwInfoConfigHw<IGFX_BROADWELL>;
#endif

} // namespace OCLRT
