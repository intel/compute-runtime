/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_ELKHARTLAKE>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    gtSystemInfo->SliceCount = 1;

    return 0;
}

template class HwInfoConfigHw<IGFX_ELKHARTLAKE>;
} // namespace NEO
