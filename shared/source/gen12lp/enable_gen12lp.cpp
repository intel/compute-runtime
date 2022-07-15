/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/cache_policy_bdw_and_later.inl"
#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_before_xe_hpc.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
template struct L1CachePolicyHelper<IGFX_TIGERLAKE_LP>;
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
static EnableCompilerHwInfoConfig<IGFX_TIGERLAKE_LP> enableCompilerHwInfoConfigTGLLP;
#endif
#ifdef SUPPORT_DG1
template struct L1CachePolicyHelper<IGFX_DG1>;
static EnableGfxProductHw<IGFX_DG1> enableGfxProductHwDG1;
static EnableCompilerHwInfoConfig<IGFX_DG1> enableCompilerHwInfoConfigDG1;
#endif
#ifdef SUPPORT_RKL
template struct L1CachePolicyHelper<IGFX_ROCKETLAKE>;
static EnableGfxProductHw<IGFX_ROCKETLAKE> enableGfxProductHwRKL;

#include "shared/source/gen12lp/compiler_hw_info_config_rkl.inl"
static EnableCompilerHwInfoConfig<IGFX_ROCKETLAKE> enableCompilerHwInfoConfigRKL;
#endif
#ifdef SUPPORT_ADLS
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_S>;
static EnableGfxProductHw<IGFX_ALDERLAKE_S> enableGfxProductHwADLS;
static EnableCompilerHwInfoConfig<IGFX_ALDERLAKE_S> enableCompilerHwInfoConfigADLS;
#endif
#ifdef SUPPORT_ADLP
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_P>;
static EnableGfxProductHw<IGFX_ALDERLAKE_P> enableGfxProductHwADLP;
static EnableCompilerHwInfoConfig<IGFX_ALDERLAKE_P> enableCompilerHwInfoConfigADLP;
#endif
#ifdef SUPPORT_ADLN
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_N>;
static EnableGfxProductHw<IGFX_ALDERLAKE_N> enableGfxProductHwADLN;
static EnableCompilerHwInfoConfig<IGFX_ALDERLAKE_N> enableCompilerHwInfoConfigADLN;
#endif
} // namespace NEO
