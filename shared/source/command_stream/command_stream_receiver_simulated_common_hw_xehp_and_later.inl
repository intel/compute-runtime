/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_base.inl"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/physical_address_allocator.h"

namespace NEO {

template <typename GfxFamily>
uint64_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    if (debugManager.flags.AUBDumpForceAllToLocalMemory.get() ||
        (gfxAllocation && gfxAllocation->getMemoryPool() == MemoryPool::localMemory)) {
        return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::localMemoryBit);
    }
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit);
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getGTTData(void *memory, AubGTTData &data) {
    data.present = true;
    data.localMemory = this->localMemoryEnabled;
}

template <typename GfxFamily>
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getMemoryBankForGtt() const {
    auto deviceIndex = getDeviceIndex();
    if (this->localMemoryEnabled) {
        return MemoryBanks::getBankForLocalMemory(deviceIndex);
    }
    return MemoryBanks::getBank(deviceIndex);
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getCsTraits(aub_stream::EngineType engineType) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineType];
}
} // namespace NEO
