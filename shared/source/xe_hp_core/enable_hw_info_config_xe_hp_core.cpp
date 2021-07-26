/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

namespace NEO {

#ifdef SUPPORT_XE_HP_SDV
static EnableProductHwInfoConfig<IGFX_XE_HP_SDV> enableXEHP;
#endif
} // namespace NEO
