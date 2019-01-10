/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_ICLLP
static EnableProductHwInfoConfig<IGFX_ICELAKE_LP> enableICLLP;
#endif
#ifdef SUPPORT_LKF
static EnableProductHwInfoConfig<IGFX_LAKEFIELD> enableLKF;
#endif

} // namespace NEO
