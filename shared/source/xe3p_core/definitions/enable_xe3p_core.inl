/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_CRI
static EnableGfxProductHw<IGFX_CRI> enableGfxProductHwCRI;
#endif

#ifdef SUPPORT_NVLP
static EnableGfxProductHw<IGFX_NVL> enableGfxProductHwNVL;
#endif
