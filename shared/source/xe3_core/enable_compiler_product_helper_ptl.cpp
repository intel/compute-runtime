/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_mtl_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hpc_and_later.inl"

#include "neo_aot_platforms.h"

constexpr auto gfxProduct = IGFX_PTL;

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getDefaultHwIpVersion() const {
    return AOT::PTL_H_B0;
}

static EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperPTL;

} // namespace NEO
