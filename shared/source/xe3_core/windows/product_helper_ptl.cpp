/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3_core/hw_cmds_ptl.h"
#include "shared/source/xe3_core/hw_info_ptl.h"

constexpr static auto gfxProduct = IGFX_PTL;

#include "shared/source/os_interface/windows/product_helper_xe2_and_later_wddm.inl"
#include "shared/source/xe3_core/os_agnostic_product_helper_xe3_core.inl"

namespace NEO {

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
