/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_bdw_and_later.inl"

namespace NEO {
typedef Gen12LpFamily Family;

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<Family>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }

    if (this->isLocalMemoryEnabled()) {
        MMIOPair lmemCfg = {0x0000cf58, 0x80000000}; //LMEM_CFG
        stream->writeMMIO(lmemCfg.first, lmemCfg.second);
    }
}

template <>
uint64_t CommandStreamReceiverSimulatedCommonHw<Family>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) |
           ((gfxAllocation && gfxAllocation->getMemoryPool() == MemoryPool::LocalMemory) ? BIT(PageTableEntry::localMemoryBit) : 0);
}

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::getGTTData(void *memory, AubGTTData &data) {
    data = {};
    data.present = true;

    data.localMemory = this->isLocalMemoryEnabled();
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
