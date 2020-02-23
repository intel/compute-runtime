/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BXT
static EnableProductHwInfoConfig<IGFX_BROXTON> enableBXT;
#endif
#ifdef SUPPORT_CFL
static EnableProductHwInfoConfig<IGFX_COFFEELAKE> enableCFL;
#endif
#ifdef SUPPORT_GLK
static EnableProductHwInfoConfig<IGFX_GEMINILAKE> enableGLK;
#endif
#ifdef SUPPORT_KBL
static EnableProductHwInfoConfig<IGFX_KABYLAKE> enableKBL;
#endif
#ifdef SUPPORT_SKL
static EnableProductHwInfoConfig<IGFX_SKYLAKE> enableSKL;
#endif

} // namespace NEO
