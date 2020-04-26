/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_bdw_plus.inl"

#include "opencl/source/aub/aub_helper_bdw_plus.inl"

namespace NEO {
typedef BDWFamily Family;

template <>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 16;
}

template <>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 2048;
    caps->image3DMaxWidth = 2048;
    caps->maxMemAllocSize = 2 * MemoryConstants::gigaByte - 8 * MemoryConstants::megaByte;
    caps->isStatelesToStatefullWithOffsetSupported = false;
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControl(LinearStream &commandStream, PipeControlArgs &args) {
    Family::PIPE_CONTROL cmd = Family::cmdInitPipeControl;
    args.dcFlushEnable = true;
    MemorySynchronizationCommands<Family>::setPipeControl(cmd, args);
    Family::PIPE_CONTROL *cmdBuffer = commandStream.getSpaceForCmd<Family::PIPE_CONTROL>();
    *cmdBuffer = cmd;
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
} // namespace NEO
