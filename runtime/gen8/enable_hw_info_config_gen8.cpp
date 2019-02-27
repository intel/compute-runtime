/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace OCLRT {

#ifdef SUPPORT_BDW
static EnableProductHwInfoConfig<IGFX_BROADWELL> enableBDW;
#endif

} // namespace OCLRT
