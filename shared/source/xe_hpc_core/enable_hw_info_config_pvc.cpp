/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

namespace NEO {

static EnableProductHwInfoConfig<IGFX_PVC> enablePVC;

} // namespace NEO
