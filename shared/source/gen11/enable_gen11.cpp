/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/cache_policy_bdw_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_ICLLP
template struct L1CachePolicyHelper<IGFX_ICELAKE_LP>;
static EnableGfxProductHw<IGFX_ICELAKE_LP> enableGfxProductHwICLLP;

#endif
#ifdef SUPPORT_LKF
template struct L1CachePolicyHelper<IGFX_LAKEFIELD>;
static EnableGfxProductHw<IGFX_LAKEFIELD> enableGfxProductHwLKF;

#endif
#ifdef SUPPORT_EHL
template struct L1CachePolicyHelper<IGFX_ELKHARTLAKE>;
static EnableGfxProductHw<IGFX_ELKHARTLAKE> enableGfxProductHwEHL;

#endif

} // namespace NEO
