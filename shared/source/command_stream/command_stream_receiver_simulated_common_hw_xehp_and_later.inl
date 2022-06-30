/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_base.inl"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/physical_address_allocator.h"

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }

    if (this->localMemoryEnabled) {
        MMIOPair guCntl = {0x00101010, 0x00000080}; //GU_CNTL
        stream->writeMMIO(guCntl.first, guCntl.second);

        MMIOPair lmemCfg = {0x0000cf58, 0x80000000}; //LMEM_CFG
        stream->writeMMIO(lmemCfg.first, lmemCfg.second);

        MMIOPair tileAddrRange[] = {{0x00004900, 0x0001},
                                    {0x00004904, 0x0001},
                                    {0x00004908, 0x0001},
                                    {0x0000490c, 0x0001}}; //XEHP_TILE_ADDR_RANGE

        const uint32_t numberOfTiles = 4;
        const uint32_t localMemorySizeGB = static_cast<uint32_t>(AubHelper::getPerTileLocalMemorySize(&this->peekHwInfo()) / MemoryConstants::gigaByte);

        uint32_t localMemoryBaseAddressInGB = 0x0;

        for (uint32_t i = 0; i < numberOfTiles; i++) {
            tileAddrRange[i].second |= localMemoryBaseAddressInGB << 1;
            tileAddrRange[i].second |= localMemorySizeGB << 8;
            stream->writeMMIO(tileAddrRange[i].first, tileAddrRange[i].second);

            localMemoryBaseAddressInGB += localMemorySizeGB;
        }
    }
}

template <typename GfxFamily>
uint64_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    if (DebugManager.flags.AUBDumpForceAllToLocalMemory.get() ||
        (gfxAllocation && gfxAllocation->getMemoryPool() == MemoryPool::LocalMemory)) {
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

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initEngineMMIO() {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[osContext->getEngineType()];
    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::submitLRCA(const MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(osContext->getEngineType()).mmioBase;
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2510), contextDescriptor.ulData[0]);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2514), contextDescriptor.ulData[1]);

    // Load our new exec list
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2550), 1);
}

} // namespace NEO
