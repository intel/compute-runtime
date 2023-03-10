/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/gen12lp/hw_info_adls.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

static EnableProductHelper<IGFX_ALDERLAKE_S> enableADLS;

} // namespace NEO
