/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen9/hw_cmds.h"
#include "runtime/helpers/enable_product.inl"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BXT
static EnableGfxProductHw<IGFX_BROXTON> enableGfxProductHwBXT;
#endif
#ifdef SUPPORT_CFL
static EnableGfxProductHw<IGFX_COFFEELAKE> enableGfxProductHwCFL;
#endif
#ifdef SUPPORT_GLK
static EnableGfxProductHw<IGFX_GEMINILAKE> enableGfxProductHwGLK;
#endif
#ifdef SUPPORT_KBL
static EnableGfxProductHw<IGFX_KABYLAKE> enableGfxProductHwKBL;
#endif
#ifdef SUPPORT_SKL
static EnableGfxProductHw<IGFX_SKYLAKE> enableGfxProductHwSKL;
#endif

} // namespace NEO
