/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_from_xe_hpg_to_xe3.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_PVC
static EnableGfxProductHw<IGFX_PVC> enableGfxProductHwPVC;
template struct L1CachePolicyHelper<IGFX_PVC>;

#endif

} // namespace NEO
