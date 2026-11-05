/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {

constexpr static auto gfxProduct = IGFX_ROCKETLAKE;

template class SysmanProductHelperHw<gfxProduct>;

static EnableSysmanProductHelper<gfxProduct> enableRkl;

} // namespace Sysman
} // namespace L0
