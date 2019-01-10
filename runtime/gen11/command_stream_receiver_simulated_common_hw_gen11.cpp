/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.inl"

namespace NEO {
typedef ICLFamily Family;

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::submitLRCA(const MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(osContext->getEngineType()).mmioBase;
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2510), contextDescriptor.ulData[0]);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2514), contextDescriptor.ulData[1]);

    // Load our new exec list
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2550), 1);
}

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO
