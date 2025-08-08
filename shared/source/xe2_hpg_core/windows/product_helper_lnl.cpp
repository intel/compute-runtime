/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"

constexpr static auto gfxProduct = IGFX_LUNARLAKE;

#include "shared/source/xe2_hpg_core/lnl/os_agnostic_product_helper_lnl.inl"
#include "shared/source/xe2_hpg_core/os_agnostic_product_helper_xe2_hpg_core.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::restartDirectSubmissionForHostptrFree() const {
    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
