/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/cache_policy_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

#ifdef SUPPORT_BXT
template struct L1CachePolicyHelper<IGFX_BROXTON>;
static EnableGfxProductHw<IGFX_BROXTON> enableGfxProductHwBXT;
#endif
#ifdef SUPPORT_CFL
template struct L1CachePolicyHelper<IGFX_COFFEELAKE>;
static EnableGfxProductHw<IGFX_COFFEELAKE> enableGfxProductHwCFL;
#endif
#ifdef SUPPORT_GLK
template struct L1CachePolicyHelper<IGFX_GEMINILAKE>;
static EnableGfxProductHw<IGFX_GEMINILAKE> enableGfxProductHwGLK;
#endif
#ifdef SUPPORT_KBL
template struct L1CachePolicyHelper<IGFX_KABYLAKE>;
static EnableGfxProductHw<IGFX_KABYLAKE> enableGfxProductHwKBL;
#endif
#ifdef SUPPORT_SKL
template struct L1CachePolicyHelper<IGFX_SKYLAKE>;
static EnableGfxProductHw<IGFX_SKYLAKE> enableGfxProductHwSKL;
#endif

} // namespace NEO
