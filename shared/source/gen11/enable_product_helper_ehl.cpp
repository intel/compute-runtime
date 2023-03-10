/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_ehl.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

static EnableProductHelper<IGFX_ELKHARTLAKE> enableEHL;

} // namespace NEO
