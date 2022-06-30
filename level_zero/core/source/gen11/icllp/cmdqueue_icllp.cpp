/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen11/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_base.inl"

#include "cmdqueue_extended.inl"
#include "igfxfmid.h"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN11_CORE>;
static CommandQueuePopulateFactory<IGFX_ICELAKE_LP, CommandQueueHw<IGFX_GEN11_CORE>> populateICLLP;

} // namespace L0
