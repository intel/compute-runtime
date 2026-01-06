/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3_core/hw_cmds_nvls.h"
#include "shared/source/xe3_core/hw_info_nvls.h"

constexpr static auto gfxProduct = IGFX_NVL_XE3G;

#include "shared/source/os_interface/windows/product_helper_xe2_and_later_wddm.inl"
#include "shared/source/xe3_core/nvls/os_agnostic_product_helper_nvls.inl"
#include "shared/source/xe3_core/os_agnostic_product_helper_xe3_core.inl"

namespace NEO {

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
