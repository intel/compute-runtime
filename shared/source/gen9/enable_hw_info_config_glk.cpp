/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_glk.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

static EnableProductProductHelper<IGFX_GEMINILAKE> enableGLK;

} // namespace NEO
