/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

static EnableProductHwInfoConfig<IGFX_ROCKETLAKE> enableRKL;

} // namespace NEO
