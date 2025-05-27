/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_base.inl"

namespace NEO {
typedef Gen12LpFamily Family;

template <typename GfxFamily>
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getMemoryBankForGtt() const {
    return MemoryBanks::getBank(getDeviceIndex());
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getCsTraits(aub_stream::EngineType engineType) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineType];
}

template <>
uint64_t CommandStreamReceiverSimulatedCommonHw<Family>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) |
           ((gfxAllocation && gfxAllocation->getMemoryPool() == MemoryPool::localMemory) ? BIT(PageTableEntry::localMemoryBit) : 0);
}

template <>
void CommandStreamReceiverSimulatedCommonHw<Family>::getGTTData(void *memory, AubGTTData &data) {
    data = {};
    data.present = true;

    data.localMemory = this->isLocalMemoryEnabled();
}

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO
