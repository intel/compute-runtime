/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_skl.h"
#include "shared/source/gen9/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_skl_to_tgllp.inl"

#include "igfxfmid.h"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN9_CORE>;
static CommandQueuePopulateFactory<IGFX_SKYLAKE, CommandQueueHw<IGFX_GEN9_CORE>> populateSKL;

} // namespace L0
