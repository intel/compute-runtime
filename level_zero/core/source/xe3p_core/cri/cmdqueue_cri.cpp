/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {
template struct CommandQueueHw<NEO::xe3pCoreEnumValue>;
static CommandQueuePopulateFactory<NEO::criProductEnumValue, CommandQueueHw<NEO::xe3pCoreEnumValue>>
    populateCRI;

} // namespace L0
