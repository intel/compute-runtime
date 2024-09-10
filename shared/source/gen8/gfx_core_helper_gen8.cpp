/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/aub_mapper.h"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_bdw_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_bdw_to_dg2.inl"
#include "shared/source/helpers/gfx_core_helper_bdw_to_icllp.inl"

namespace NEO {
typedef Gen8Family Family;

template <>
size_t GfxCoreHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 16;
}

template <>
size_t GfxCoreHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 2048;
}

template <>
uint64_t GfxCoreHelperHw<Family>::getMaxMemAllocSize() const {
    return (2 * MemoryConstants::gigaByte) - (8 * MemoryConstants::kiloByte);
}

template <>
bool GfxCoreHelperHw<Family>::isStatelessToStatefulWithOffsetSupported() const {
    return false;
}

template <>
void MemorySynchronizationCommands<Family>::addSingleBarrier(LinearStream &commandStream, PipeControlArgs &args) {
    Family::PIPE_CONTROL cmd = Family::cmdInitPipeControl;
    MemorySynchronizationCommands<Family>::setSingleBarrier(&cmd, args);

    cmd.setDcFlushEnable(true);

    if (debugManager.flags.DoNotFlushCaches.get()) {
        cmd.setDcFlushEnable(false);
    }

    Family::PIPE_CONTROL *cmdBuffer = commandStream.getSpaceForCmd<Family::PIPE_CONTROL>();
    *cmdBuffer = cmd;
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
