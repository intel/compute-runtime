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
#include "shared/source/helpers/compiler_product_helper_before_xe_hp.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_disable_split_matrix_multiply_accumulate.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"

#include "compiler_product_helper_adlp.inl"
#include "platforms.h"

namespace NEO {

template <>
uint32_t CompilerProductHelperHw<IGFX_ALDERLAKE_P>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::ADL_P;
}

static EnableCompilerProductHelper<IGFX_ALDERLAKE_P> enableCompilerProductHelperADLP;

} // namespace NEO
