/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "core/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_TGLLP
static EnableProductHwInfoConfig<IGFX_TIGERLAKE_LP> enableTGLLP;
#endif

} // namespace NEO
