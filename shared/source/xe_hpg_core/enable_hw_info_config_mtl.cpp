/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

namespace NEO {

static EnableProductProductHelper<IGFX_METEORLAKE> enableMTL;

} // namespace NEO
