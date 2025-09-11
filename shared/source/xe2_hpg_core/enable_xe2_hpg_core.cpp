/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_from_xe_hpg_to_xe3.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_BMG
template struct L1CachePolicyHelper<IGFX_BMG>;
static EnableGfxProductHw<IGFX_BMG> enableGfxProductHwBMG;

#endif

#ifdef SUPPORT_LNL
template struct L1CachePolicyHelper<IGFX_LUNARLAKE>;
static EnableGfxProductHw<IGFX_LUNARLAKE> enableGfxProductHwLNL;
#endif

} // namespace NEO
