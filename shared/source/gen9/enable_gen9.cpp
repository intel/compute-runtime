/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/cache_policy_bdw_and_later.inl"
#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_before_xe_hpc.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BXT
template struct L1CachePolicyHelper<IGFX_BROXTON>;
static EnableGfxProductHw<IGFX_BROXTON> enableGfxProductHwBXT;
static EnableCompilerHwInfoConfig<IGFX_BROXTON> enableCompilerHwInfoConfigBXT;
#endif
#ifdef SUPPORT_CFL
template struct L1CachePolicyHelper<IGFX_COFFEELAKE>;
static EnableGfxProductHw<IGFX_COFFEELAKE> enableGfxProductHwCFL;
static EnableCompilerHwInfoConfig<IGFX_COFFEELAKE> enableCompilerHwInfoConfigCFL;
#endif
#ifdef SUPPORT_GLK
template struct L1CachePolicyHelper<IGFX_GEMINILAKE>;
static EnableGfxProductHw<IGFX_GEMINILAKE> enableGfxProductHwGLK;
static EnableCompilerHwInfoConfig<IGFX_GEMINILAKE> enableCompilerHwInfoConfigGLK;
#endif
#ifdef SUPPORT_KBL
template struct L1CachePolicyHelper<IGFX_KABYLAKE>;
static EnableGfxProductHw<IGFX_KABYLAKE> enableGfxProductHwKBL;
static EnableCompilerHwInfoConfig<IGFX_KABYLAKE> enableCompilerHwInfoConfigKBL;
#endif
#ifdef SUPPORT_SKL
template struct L1CachePolicyHelper<IGFX_SKYLAKE>;
static EnableGfxProductHw<IGFX_SKYLAKE> enableGfxProductHwSKL;
static EnableCompilerHwInfoConfig<IGFX_SKYLAKE> enableCompilerHwInfoConfigSKL;
#endif

} // namespace NEO
