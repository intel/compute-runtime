/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_base.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"

namespace NEO {
typedef Gen12LpFamily Family;

template <typename GfxFamily>
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getMemoryBankForGtt() const {
    return MemoryBanks::getBank(getDeviceIndex());
}

template <>
uint64_t CommandStreamReceiverSimulatedCommonHw<Family>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) |
           ((gfxAllocation && gfxAllocation->getMemoryPool() == MemoryPool::localMemory) ? BIT(PageTableEntry::localMemoryBit) : 0);
}

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO
