/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_icllp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ICELAKE_LP;

#include "shared/source/gen11/icllp/os_agnostic_hw_info_config_icllp.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (nullptr == osIface) {
        return 0;
    }

    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    gtSystemInfo->SliceCount = 1;

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
