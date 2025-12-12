/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/xe3p_core/cmdlist_xe3p_core.h"

namespace L0 {

static CommandListPopulateFactory<NEO::criProductEnumValue, CommandListProductFamily<NEO::criProductEnumValue>>
    populateCRI;

static CommandListImmediatePopulateFactory<NEO::criProductEnumValue, CommandListImmediateProductFamily<NEO::criProductEnumValue>>
    populateCRIImmediate;

} // namespace L0
