/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pipe_control_args.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                    const size_t *pRangeSizes,
                                                                    const void **pRanges) {
    const auto &hwInfo = this->device->getHwInfo();
    NEO::PipeControlArgs args;
    args.dcFlushEnable = NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed(true, hwInfo);
    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(),
                                                                  args);
}

} // namespace L0
