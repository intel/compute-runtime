/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

namespace NEO {

static EnableProductHelper<IGFX_METEORLAKE> enableMTL;

} // namespace NEO
