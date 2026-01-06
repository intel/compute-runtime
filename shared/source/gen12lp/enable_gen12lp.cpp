/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/cache_policy_gen12lp.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
template struct L1CachePolicyHelper<IGFX_TIGERLAKE_LP>;
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;

#endif
#ifdef SUPPORT_DG1
template struct L1CachePolicyHelper<IGFX_DG1>;
static EnableGfxProductHw<IGFX_DG1> enableGfxProductHwDG1;

#endif
#ifdef SUPPORT_RKL
template struct L1CachePolicyHelper<IGFX_ROCKETLAKE>;
static EnableGfxProductHw<IGFX_ROCKETLAKE> enableGfxProductHwRKL;

#endif
#ifdef SUPPORT_ADLS
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_S>;
static EnableGfxProductHw<IGFX_ALDERLAKE_S> enableGfxProductHwADLS;

#endif
#ifdef SUPPORT_ADLP
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_P>;
static EnableGfxProductHw<IGFX_ALDERLAKE_P> enableGfxProductHwADLP;

#endif
#ifdef SUPPORT_ADLN
template struct L1CachePolicyHelper<IGFX_ALDERLAKE_N>;
static EnableGfxProductHw<IGFX_ALDERLAKE_N> enableGfxProductHwADLN;
#endif
} // namespace NEO
