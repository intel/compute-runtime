/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw_bdw_plus.inl"
#include "opencl/source/gen12lp/helpers_gen12lp.h"

namespace NEO {
typedef TGLLPFamily Family;

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<Family>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }

    Gen12LPHelpers::initAdditionalGlobalMMIO(*this, *stream);
}

template <>
uint64_t CommandStreamReceiverSimulatedCommonHw<Family>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | Gen12LPHelpers::getPPGTTAdditionalBits(gfxAllocation);
}

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::getGTTData(void *memory, AubGTTData &data) {
    data = {};
    data.present = true;

    Gen12LPHelpers::adjustAubGTTData(*this, data);
}

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
