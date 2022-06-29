/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

static EnableProductHwInfoConfig<IGFX_LAKEFIELD> enableLKF;

} // namespace NEO
