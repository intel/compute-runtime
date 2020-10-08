/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/source/helpers/enable_product.inl"

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
} // namespace NEO
