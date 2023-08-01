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
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_product_config_default.inl"

#include "platforms.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_LAKEFIELD>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100080008;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_LAKEFIELD>::getDefaultHwIpVersion() const {
    return AOT::LKF;
}

static EnableCompilerProductHelper<IGFX_LAKEFIELD> enableCompilerProductHelperLKF;

} // namespace NEO
