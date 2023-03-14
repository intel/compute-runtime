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
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_ELKHARTLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100040008;
}

static EnableCompilerProductHelper<IGFX_ELKHARTLAKE> enableCompilerProductHelperEHL;

} // namespace NEO
