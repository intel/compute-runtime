/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"

namespace NEO {
#ifdef SUPPORT_DG2
static EnableGfxProductHw<IGFX_DG2> enableGfxProductHwDG2;
#endif

#ifdef SUPPORT_MTL
static EnableGfxProductHw<IGFX_METEORLAKE> enableGfxProductHwMTL;
#endif

#ifdef SUPPORT_ARL
static EnableGfxProductHw<IGFX_ARROWLAKE> enableGfxProductHwARL;
#endif

} // namespace NEO
