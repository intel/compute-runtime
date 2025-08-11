/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/gen12lp/hw_info_adls.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_gen12lp.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN12LP_CORE>;
static CommandQueuePopulateFactory<IGFX_ALDERLAKE_S, CommandQueueHw<IGFX_GEN12LP_CORE>>
    populateADLS;

} // namespace L0
