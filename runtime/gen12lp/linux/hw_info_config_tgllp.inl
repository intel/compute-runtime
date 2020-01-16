/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "core/os_interface/hw_info_config.h"

namespace NEO {

template <>
int HwInfoConfigHw<IGFX_TIGERLAKE_LP>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    hwInfo->capabilityTable.ftrRenderCompressedImages = hwInfo->featureTable.ftrE2ECompression;
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = hwInfo->featureTable.ftrE2ECompression;

    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;
    return 0;
}

template class HwInfoConfigHw<IGFX_TIGERLAKE_LP>;
} // namespace NEO
