/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_bdw_plus.inl"
#include "shared/source/memory_manager/memory_constants.h"
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
typename Family::PIPE_CONTROL *MemorySynchronizationCommands<Family>::addPipeControl(LinearStream &commandStream, bool dcFlush) {
    return MemorySynchronizationCommands<Family>::obtainPipeControl(commandStream, true);
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
} // namespace NEO
