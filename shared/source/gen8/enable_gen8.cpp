/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/cache_policy_bdw_and_later.inl"
#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_before_xe_hpc.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BDW
template struct L1CachePolicyHelper<IGFX_BROADWELL>;
static EnableGfxProductHw<IGFX_BROADWELL> enableGfxProductHwBDW;

#include "shared/source/gen8/compiler_hw_info_config_bdw.inl"
static EnableCompilerHwInfoConfig<IGFX_BROADWELL> enableCompilerHwInfoConfigBDW;
#endif

} // namespace NEO
