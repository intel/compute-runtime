/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/compiler_product_helper_gen12lp.inl"
#include "shared/source/helpers/compiler_aot_config_before_mtl.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_product_config_default.inl"

#include "platforms.h"

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<IGFX_ALDERLAKE_N>::getDefaultHwIpVersion() const {
    return AOT::ADL_N;
}

template <>
uint64_t CompilerProductHelperHw<IGFX_ALDERLAKE_N>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100020010;
}

static EnableCompilerProductHelper<IGFX_ALDERLAKE_N> enableCompilerProductHelperADLN;

} // namespace NEO
