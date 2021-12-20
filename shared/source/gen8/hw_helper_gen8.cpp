/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/aub_mapper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/hw_helper_base.inl"
#include "shared/source/helpers/hw_helper_bdw_and_later.inl"
#include "shared/source/helpers/hw_helper_bdw_to_icllp.inl"

namespace NEO {
typedef BDWFamily Family;

static uint32_t slmSizeId[] = {0, 1, 2, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16};

template <>
uint32_t HwHelperHw<Family>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 4096u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    return slmSize;
}

template <>
uint32_t HwHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) {
    slmSize += (4 * KB - 1);
    slmSize = slmSize >> 12;
    slmSize = std::min(slmSize, 15u);
    slmSize = slmSizeId[slmSize];
    return slmSize;
}

template <>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 16;
}

template <>
size_t HwHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 2048;
}

template <>
uint64_t HwHelperHw<Family>::getMaxMemAllocSize() const {
    return (2 * MemoryConstants::gigaByte) - (8 * MemoryConstants::kiloByte);
}

template <>
bool HwHelperHw<Family>::isStatelesToStatefullWithOffsetSupported() const {
    return false;
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControl(LinearStream &commandStream, PipeControlArgs &args) {
    Family::PIPE_CONTROL cmd = Family::cmdInitPipeControl;
    args.dcFlushEnable = true;
    MemorySynchronizationCommands<Family>::setPipeControl(cmd, args);
    Family::PIPE_CONTROL *cmdBuffer = commandStream.getSpaceForCmd<Family::PIPE_CONTROL>();
    *cmdBuffer = cmd;
}

template <>
void MemorySynchronizationCommands<Family>::addPipeControlWithCSStallOnly(LinearStream &commandStream) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    PIPE_CONTROL cmd = Family::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    cmd.setDcFlushEnable(true);
    auto pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = cmd;
}

template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
