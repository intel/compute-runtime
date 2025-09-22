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
uint64_t CompilerProductHelperHw<IGFX_ROCKETLAKE>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100020010;
}
template <>
bool CompilerProductHelperHw<IGFX_ROCKETLAKE>::isForceEmuInt32DivRemSPRequired() const {
    return true;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_ROCKETLAKE>::getDefaultHwIpVersion() const {
    return AOT::RKL;
}

template <>
bool CompilerProductHelperHw<IGFX_ROCKETLAKE>::oclocEnforceZebinFormat() const {
    return true;
}

static EnableCompilerProductHelper<IGFX_ROCKETLAKE> enableCompilerProductHelperRKL;

} // namespace NEO
