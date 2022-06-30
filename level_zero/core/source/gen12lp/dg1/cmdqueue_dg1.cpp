/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_base.inl"

#include "cmdqueue_extended.inl"
#include "igfxfmid.h"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN12LP_CORE>;
static CommandQueuePopulateFactory<IGFX_DG1, CommandQueueHw<IGFX_GEN12LP_CORE>>
    populateDG1;

} // namespace L0