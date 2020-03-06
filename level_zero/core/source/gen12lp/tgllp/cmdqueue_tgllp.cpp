/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue_hw_base.inl"

#include "cmdqueue_extended.inl"
#include "igfxfmid.h"

namespace L0 {

static CommandQueuePopulateFactory<IGFX_TIGERLAKE_LP, CommandQueueHw<IGFX_GEN12LP_CORE>>
    populateTGLLP;

} // namespace L0
