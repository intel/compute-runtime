/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/hw_info_config.h"

namespace OCLRT {

#ifdef SUPPORT_CNL
static EnableProductHwInfoConfig<IGFX_CANNONLAKE> enableCNL;
#endif

} // namespace OCLRT
