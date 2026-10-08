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
constexpr static auto gfxProduct = IGFX_ALDERLAKE_S;

template <>
std::string SysmanProductHelperHw<gfxProduct>::getGpuGeneration() {
    return "xe";
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
