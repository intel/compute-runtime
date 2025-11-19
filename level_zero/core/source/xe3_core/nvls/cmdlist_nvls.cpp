/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_info_xe3_core.h"

#include "level_zero/core/source/xe3_core/cmdlist_xe3_core.h"

namespace L0 {

static CommandListPopulateFactory<NEO::nvlsProductEnumValue, CommandListProductFamily<NEO::nvlsProductEnumValue>>
    populateNVLS;

static CommandListImmediatePopulateFactory<NEO::nvlsProductEnumValue, CommandListImmediateProductFamily<NEO::nvlsProductEnumValue>>
    populateNVLSImmediate;

} // namespace L0
