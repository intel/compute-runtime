/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.h"

#include "hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_CNL
static EnableProductHwInfoConfig<IGFX_CANNONLAKE> enableCNL;
#endif

} // namespace NEO
