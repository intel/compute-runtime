/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"

namespace NEO {

#ifdef SUPPORT_BMG
static EnableGfxProductHw<IGFX_BMG> enableGfxProductHwBMG;
#endif

#ifdef SUPPORT_LNL
static EnableGfxProductHw<IGFX_LUNARLAKE> enableGfxProductHwLNL;
#endif

} // namespace NEO
