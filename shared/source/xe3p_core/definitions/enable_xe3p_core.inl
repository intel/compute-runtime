/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_CRI
template struct L1CachePolicyHelper<IGFX_CRI>;
static EnableGfxProductHw<IGFX_CRI> enableGfxProductHwCRI;
#endif
