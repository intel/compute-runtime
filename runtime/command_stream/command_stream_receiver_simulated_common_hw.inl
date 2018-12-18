/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "aub_mapper.h"

namespace OCLRT {

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
MMIOList CommandStreamReceiverSimulatedCommonHw<GfxFamily>::splitMMIORegisters(const std::string &registers, char delimiter) {
    MMIOList result;
    bool firstElementInPair = false;
    std::string token;
    uint32_t registerOffset = 0;
    uint32_t registerValue = 0;
    std::istringstream stream("");

    for (std::string::const_iterator i = registers.begin();; i++) {
        if (i == registers.end() || *i == delimiter) {
            if (token.size() > 0) {
                stream.str(token);
                stream.clear();
                firstElementInPair = !firstElementInPair;
                stream >> std::hex >> (firstElementInPair ? registerOffset : registerValue);
                if (stream.fail()) {
                    result.clear();
                    break;
                }
                token.clear();
                if (!firstElementInPair) {
                    result.push_back(std::pair<uint32_t, uint32_t>(registerOffset, registerValue));
                    registerValue = 0;
                    registerOffset = 0;
                }
            }
            if (i == registers.end()) {
                break;
            }
        } else {
            token.push_back(*i);
        }
    }
    return result;
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO() {
    if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
        auto mmioList = splitMMIORegisters(DebugManager.flags.AubDumpAddMmioRegistersList.get(), ';');
        for (auto &mmioPair : mmioList) {
            stream->writeMMIO(mmioPair.first, mmioPair.second);
        }
    }
}

template <typename GfxFamily>
uint64_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::userSupervisorBit);
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getGTTData(void *memory, AubGTTData &data) {
    data.present = true;
    data.localMemory = false;
}

template <typename GfxFamily>
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getMemoryBankForGtt() const {
    return MemoryBanks::getBank(this->deviceIndex);
}

template <typename GfxFamily>
size_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getEngineIndex(EngineInstanceT engineInstance) {
    auto findCriteria = [&](const auto &it) { return it.type == engineInstance.type && it.id == engineInstance.id; };
    auto findResult = std::find_if(allEngineInstances.begin(), allEngineInstances.end(), findCriteria);
    UNRECOVERABLE_IF(findResult == allEngineInstances.end());
    return findResult - allEngineInstances.begin();
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getCsTraits(EngineInstanceT engineInstance) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineInstance.type];
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initEngineMMIO(EngineInstanceT engineInstance) {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[engineInstance.type];

    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::submitLRCA(EngineInstanceT engineInstance, const MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(engineInstance).mmioBase;
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), 0);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), 0);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), contextDescriptor.ulData[1]);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), contextDescriptor.ulData[0]);
}
} // namespace OCLRT
