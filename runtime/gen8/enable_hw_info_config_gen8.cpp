/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BDW
static EnableProductHwInfoConfig<IGFX_BROADWELL> enableBDW;
#endif

} // namespace NEO
