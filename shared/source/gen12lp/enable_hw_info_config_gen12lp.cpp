/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableProductHwInfoConfig<IGFX_TIGERLAKE_LP> enableTGLLP;
#endif
#ifdef SUPPORT_DG1
static EnableProductHwInfoConfig<IGFX_DG1> enableDG1;
#endif
#ifdef SUPPORT_RKL
static EnableProductHwInfoConfig<IGFX_ROCKETLAKE> enableRKL;
#endif
#ifdef SUPPORT_ADLS
static EnableProductHwInfoConfig<IGFX_ALDERLAKE_S> enableADLS;
#endif
} // namespace NEO
