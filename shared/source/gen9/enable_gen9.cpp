/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BXT
static EnableGfxProductHw<IGFX_BROXTON> enableGfxProductHwBXT;
static EnableCompilerHwInfoConfig<IGFX_BROXTON> enableCompilerHwInfoConfigBXT;
#endif
#ifdef SUPPORT_CFL
static EnableGfxProductHw<IGFX_COFFEELAKE> enableGfxProductHwCFL;
static EnableCompilerHwInfoConfig<IGFX_COFFEELAKE> enableCompilerHwInfoConfigCFL;
#endif
#ifdef SUPPORT_GLK
static EnableGfxProductHw<IGFX_GEMINILAKE> enableGfxProductHwGLK;
static EnableCompilerHwInfoConfig<IGFX_GEMINILAKE> enableCompilerHwInfoConfigGLK;
#endif
#ifdef SUPPORT_KBL
static EnableGfxProductHw<IGFX_KABYLAKE> enableGfxProductHwKBL;
static EnableCompilerHwInfoConfig<IGFX_KABYLAKE> enableCompilerHwInfoConfigKBL;
#endif
#ifdef SUPPORT_SKL
static EnableGfxProductHw<IGFX_SKYLAKE> enableGfxProductHwSKL;
static EnableCompilerHwInfoConfig<IGFX_SKYLAKE> enableCompilerHwInfoConfigSKL;
#endif

} // namespace NEO
