/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen9/hw_info.h"
#include "shared/source/helpers/pipe_control_args.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template struct CommandListCoreFamily<IGFX_GEN9_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_GEN9_CORE>;

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                    const size_t *pRangeSizes,
                                                                    const void **pRanges) {
    const auto &hwInfo = this->device->getHwInfo();
    NEO::PipeControlArgs args;
    args.dcFlushEnable = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(),
                                                                  args);
}
} // namespace L0
