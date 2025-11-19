/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3_core/hw_cmds_nvls.h"
#include "shared/source/xe3_core/hw_info_nvls.h"

namespace NEO {

static EnableProductHelper<nvlsProductEnumValue> enableNVLS;

} // namespace NEO
