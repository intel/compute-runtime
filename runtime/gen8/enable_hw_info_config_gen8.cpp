/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/hw_info_config.h"

namespace OCLRT {

#ifdef SUPPORT_BDW
static EnableProductHwInfoConfig<IGFX_BROADWELL> enableBDW;
#endif

} // namespace OCLRT
