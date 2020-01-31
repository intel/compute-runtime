/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "core/os_interface/hw_info_config.h"

namespace NEO {

#ifdef SUPPORT_BDW
static EnableProductHwInfoConfig<IGFX_BROADWELL> enableBDW;
#endif

} // namespace NEO
