/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_PTL
template struct L1CachePolicyHelper<IGFX_PTL>;
static EnableGfxProductHw<IGFX_PTL> enableGfxProductHwPTL;
#endif
