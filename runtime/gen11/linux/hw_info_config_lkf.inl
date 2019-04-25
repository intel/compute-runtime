/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_LAKEFIELD>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);

    pSysInfo->SliceCount = 1;

    return 0;
}

template class HwInfoConfigHw<IGFX_LAKEFIELD>;
} // namespace NEO
