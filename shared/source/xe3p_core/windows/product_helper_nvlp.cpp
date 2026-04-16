/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_nvlp.h"

constexpr static auto gfxProduct = IGFX_NVL;

#include "shared/source/os_interface/windows/product_helper_xe2_and_later_wddm.inl"
#include "shared/source/os_interface/windows/product_helper_xe3p_and_later_wddm.inl"
#include "shared/source/xe3p_core/nvlp/os_agnostic_product_helper_nvlp.inl"
#include "shared/source/xe3p_core/os_agnostic_product_helper_xe3p_core.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::areSecondaryContextsSupported() const {
    return true;
}

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
