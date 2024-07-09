/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_and_later.inl"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/ptr_math.h"

namespace NEO {
using Family = Xe2HpgCoreFamily;

template <>
bool CommandStreamReceiverSimulatedCommonHw<Family>::expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    auto gpuAddress = peekGmmHelper()->decanonize(castToUint64(gfxAddress));
    return this->expectMemory(reinterpret_cast<void *>(gpuAddress), srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);
}
template <>
bool CommandStreamReceiverSimulatedCommonHw<Family>::expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    auto gpuAddress = peekGmmHelper()->decanonize(castToUint64(gfxAddress));
    return this->expectMemory(reinterpret_cast<void *>(gpuAddress), srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);
}
template <>
bool CommandStreamReceiverSimulatedCommonHw<Family>::expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length) {
    auto gpuAddress = peekGmmHelper()->decanonize(castToUint64(gfxAddress));
    return this->expectMemory(reinterpret_cast<void *>(gpuAddress), srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);
}

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO
