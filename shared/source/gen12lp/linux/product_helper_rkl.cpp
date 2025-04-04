/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/gen12lp/hw_info_rkl.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_ROCKETLAKE;

#include "shared/source/gen12lp/os_agnostic_product_helper_gen12lp.inl"
#include "shared/source/gen12lp/rkl/os_agnostic_product_helper_rkl.inl"
namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
    gtSystemInfo->SliceCount = 1;

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
