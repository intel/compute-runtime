/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"

namespace NEO {
#ifdef SUPPORT_PTL
static EnableGfxProductHw<IGFX_PTL> enableGfxProductHwPTL;
#endif

#ifdef SUPPORT_NVLS
static EnableGfxProductHw<IGFX_NVL_XE3G> enableGfxProductHwNVLS;
#endif
} // namespace NEO
