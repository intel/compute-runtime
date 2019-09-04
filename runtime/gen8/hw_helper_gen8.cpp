/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "runtime/aub/aub_helper_bdw_plus.inl"
#include "runtime/helpers/flat_batch_buffer_helper_hw.inl"
#include "runtime/helpers/hw_helper_bdw_plus.inl"

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
typename Family::PIPE_CONTROL *PipeControlHelper<Family>::addPipeControl(LinearStream &commandStream, bool dcFlush) {
    return PipeControlHelper<Family>::obtainPipeControl(commandStream, true);
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct PipeControlHelper<Family>;
} // namespace NEO
