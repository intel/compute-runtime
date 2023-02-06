/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

namespace NEO {

static EnableProductProductHelper<IGFX_PVC> enablePVC;

} // namespace NEO
