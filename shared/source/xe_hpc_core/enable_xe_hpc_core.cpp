/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_dg2_and_later.inl"
#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_xe_hpc_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_PVC
static EnableGfxProductHw<IGFX_PVC> enableGfxProductHwPVC;
template struct L1CachePolicyHelper<IGFX_PVC>;
static EnableCompilerHwInfoConfig<IGFX_PVC> enableCompilerHwInfoConfigPVC;
#endif

} // namespace NEO
