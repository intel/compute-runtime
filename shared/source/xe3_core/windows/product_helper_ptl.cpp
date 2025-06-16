/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3_core/hw_cmds_ptl.h"
#include "shared/source/xe3_core/hw_info_ptl.h"

constexpr static auto gfxProduct = IGFX_PTL;

#include "shared/source/xe3_core/os_agnostic_product_helper_xe3_core.inl"
#include "shared/source/xe3_core/ptl/os_agnostic_product_helper_ptl.inl"

namespace NEO {

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
