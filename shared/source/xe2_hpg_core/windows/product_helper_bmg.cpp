/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_bmg.h"

constexpr static auto gfxProduct = IGFX_BMG;

#include "shared/source/helpers/windows/product_helper_dg2_and_later_discrete.inl"
#include "shared/source/xe2_hpg_core/bmg/os_agnostic_product_helper_bmg.inl"
#include "shared/source/xe2_hpg_core/os_agnostic_product_helper_xe2_hpg_core.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::restartDirectSubmissionForHostptrFree() const {
    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
