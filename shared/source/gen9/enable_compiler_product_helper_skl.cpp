/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_to_icllp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_disable_split_matrix_multiply_accumulate.inl"
#include "shared/source/helpers/compiler_product_helper_disable_subgroup_local_block_io.inl"

#include "platforms.h"

namespace NEO {

template <>
uint64_t CompilerProductHelperHw<IGFX_SKYLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100030008;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_SKYLAKE>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::SKL;
}

static EnableCompilerProductHelper<IGFX_SKYLAKE> enableCompilerProductHelperSKL;

} // namespace NEO
