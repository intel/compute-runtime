/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_mtl_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hpc_and_later.inl"

#include "neo_aot_platforms.h"
constexpr auto gfxProduct = IGFX_CRI;
#include "shared/source/xe3p_core/compiler_product_helper_xe3p.inl"
namespace NEO {
template <>
uint32_t CompilerProductHelperHw<gfxProduct>::getDefaultHwIpVersion() const {
    return AOT::CRI_A0;
}

static EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperCRI;

} // namespace NEO
