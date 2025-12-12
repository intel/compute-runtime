/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_cri.h"

namespace NEO {

static EnableProductHelper<NEO::criProductEnumValue> enableCRI;

} // namespace NEO
