/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

namespace NEO {

static EnableProductHelper<IGFX_ARROWLAKE> enableARL;

} // namespace NEO
