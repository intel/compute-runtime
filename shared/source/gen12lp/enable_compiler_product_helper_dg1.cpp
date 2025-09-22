/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_product_config_default.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"

#include "neo_aot_platforms.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_DG1>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100060010;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_DG1>::getDefaultHwIpVersion() const {
    return AOT::DG1;
}

static EnableCompilerProductHelper<IGFX_DG1> enableCompilerProductHelperDG1;

} // namespace NEO
