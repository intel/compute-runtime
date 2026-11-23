/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableGfxProductHw<IGFX_TIGERLAKE_LP> enableGfxProductHwTGLLP;
#endif
#ifdef SUPPORT_DG1
static EnableGfxProductHw<IGFX_DG1> enableGfxProductHwDG1;
#endif
#ifdef SUPPORT_RKL
static EnableGfxProductHw<IGFX_ROCKETLAKE> enableGfxProductHwRKL;
#endif
#ifdef SUPPORT_ADLS
static EnableGfxProductHw<IGFX_ALDERLAKE_S> enableGfxProductHwADLS;
#endif
#ifdef SUPPORT_ADLP
static EnableGfxProductHw<IGFX_ALDERLAKE_P> enableGfxProductHwADLP;
#endif
#ifdef SUPPORT_ADLN
static EnableGfxProductHw<IGFX_ALDERLAKE_N> enableGfxProductHwADLN;
#endif
} // namespace NEO
