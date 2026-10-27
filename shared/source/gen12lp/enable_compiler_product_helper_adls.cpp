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
#include "shared/source/helpers/compiler_product_helper_product_config_default.inl"

#include "platforms.h"

namespace NEO {
template <>
uint64_t CompilerProductHelperHw<IGFX_ALDERLAKE_S>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x100020010;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_ALDERLAKE_S>::getDefaultHwIpVersion() const {
    return AOT::ADL_S;
}

static EnableCompilerProductHelper<IGFX_ALDERLAKE_S> enableCompilerProductHelperADLS;

} // namespace NEO
