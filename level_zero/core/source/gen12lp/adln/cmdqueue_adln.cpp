/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_base.inl"

#include "cmdqueue_extended.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN12LP_CORE>;
static CommandQueuePopulateFactory<IGFX_ALDERLAKE_N, CommandQueueHw<IGFX_GEN12LP_CORE>>
    populateADLN;

} // namespace L0
