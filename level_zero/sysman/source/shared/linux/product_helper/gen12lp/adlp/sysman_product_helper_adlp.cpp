/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_ALDERLAKE_P;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_from_gen12lp_to_xe_hpc.inl"

template class SysmanProductHelperHw<gfxProduct>;

static EnableSysmanProductHelper<gfxProduct> enableAdlp;

} // namespace Sysman
} // namespace L0
