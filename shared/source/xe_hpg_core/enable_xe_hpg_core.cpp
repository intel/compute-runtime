/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {
#ifdef SUPPORT_DG2
static EnableGfxProductHw<IGFX_DG2> enableGfxProductHwDG2;

#include "shared/source/xe_hpg_core/compiler_hw_info_config_dg2.inl"
static EnableCompilerHwInfoConfig<IGFX_DG2> enableCompilerHwInfoConfigDG2;
#endif
} // namespace NEO
