/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BDW
static EnableGfxProductHw<IGFX_BROADWELL> enableGfxProductHwBDW;
static EnableCompilerHwInfoConfig<IGFX_BROADWELL> enableCompilerHwInfoConfigBDW;
#endif

} // namespace NEO
