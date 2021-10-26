/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
static EnableCompilerHwInfoConfig<IGFX_TIGERLAKE_LP> enableCompilerHwInfoConfigTGLLP;
#endif
#ifdef SUPPORT_DG1
static EnableGfxProductHw<IGFX_DG1> enableGfxProductHwDG1;
static EnableCompilerHwInfoConfig<IGFX_DG1> enableCompilerHwInfoConfigDG1;
#endif
#ifdef SUPPORT_RKL
static EnableGfxProductHw<IGFX_ROCKETLAKE> enableGfxProductHwRKL;

#include "shared/source/gen12lp/compiler_hw_info_config_rkl.inl"
static EnableCompilerHwInfoConfig<IGFX_ROCKETLAKE> enableCompilerHwInfoConfigRKL;
#endif
#ifdef SUPPORT_ADLS
static EnableGfxProductHw<IGFX_ALDERLAKE_S> enableGfxProductHwADLS;
static EnableCompilerHwInfoConfig<IGFX_ALDERLAKE_S> enableCompilerHwInfoConfigADLS;
#endif
#ifdef SUPPORT_ADLP
static EnableGfxProductHw<IGFX_ALDERLAKE_P> enableGfxProductHwADLP;
static EnableCompilerHwInfoConfig<IGFX_ALDERLAKE_P> enableCompilerHwInfoConfigADLP;
#endif
} // namespace NEO
