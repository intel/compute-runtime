/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

#include "cmdqueue_extended.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_XE_HPC_CORE>;
static CommandQueuePopulateFactory<IGFX_PVC, CommandQueueHw<IGFX_XE_HPC_CORE>>
    populatePVC;

} // namespace L0
