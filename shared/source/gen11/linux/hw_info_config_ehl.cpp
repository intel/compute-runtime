/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_ehl.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_ELKHARTLAKE;

#include "shared/source/gen11/ehl/os_agnostic_product_helper_ehl.inl"
namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (nullptr == osIface) {
        return 0;
    }
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    gtSystemInfo->SliceCount = 1;

    return 0;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
