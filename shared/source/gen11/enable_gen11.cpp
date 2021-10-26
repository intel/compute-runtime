/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_ICLLP
static EnableGfxProductHw<IGFX_ICELAKE_LP> enableGfxProductHwICLLP;
static EnableCompilerHwInfoConfig<IGFX_ICELAKE_LP> enableCompilerHwInfoConfigICLLP;
#endif
#ifdef SUPPORT_LKF
static EnableGfxProductHw<IGFX_LAKEFIELD> enableGfxProductHwLKF;
static EnableCompilerHwInfoConfig<IGFX_LAKEFIELD> enableCompilerHwInfoConfigLKF;
#endif
#ifdef SUPPORT_EHL
static EnableGfxProductHw<IGFX_ELKHARTLAKE> enableGfxProductHwEHL;
static EnableCompilerHwInfoConfig<IGFX_ELKHARTLAKE> enableCompilerHwInfoConfigEHL;
#endif

} // namespace NEO
