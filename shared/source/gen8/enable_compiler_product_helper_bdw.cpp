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
#include "shared/source/helpers/compiler_product_helper_disable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_product_config_default.inl"

#include "platforms.h"

namespace NEO {
template <>
uint32_t CompilerProductHelperHw<IGFX_BROADWELL>::getDefaultHwIpVersion() const {
    return AOT::BDW;
}

template <>
bool CompilerProductHelperHw<IGFX_BROADWELL>::isStatelessToStatefulBufferOffsetSupported() const {
    return false;
}

template <>
uint64_t CompilerProductHelperHw<IGFX_BROADWELL>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100030008;
}

static EnableCompilerProductHelper<IGFX_BROADWELL> enableCompilerProductHelperBDW;

} // namespace NEO
