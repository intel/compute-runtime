/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<NEO::criProductEnumValue, KernelHw<NEO::xe3pCoreEnumValue>> populateCRI;

} // namespace L0
