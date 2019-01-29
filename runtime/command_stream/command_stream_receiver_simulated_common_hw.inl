/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"
#include "third_party/aub_stream/headers/aub_manager.h"
#include "aub_mapper.h"

namespace OCLRT {

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO() {
    if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
        auto mmioList = AubHelper::getAdditionalMmioList();
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
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getEngineIndex(EngineInstanceT engineInstance) {
    auto findCriteria = [&](const auto &it) { return it.type == engineInstance.type && it.id == engineInstance.id; };
    auto findResult = std::find_if(allEngineInstances.begin(), allEngineInstances.end(), findCriteria);
    UNRECOVERABLE_IF(findResult == allEngineInstances.end());
    return static_cast<uint32_t>(findResult - allEngineInstances.begin());
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getCsTraits(EngineInstanceT engineInstance) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineInstance.type];
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initEngineMMIO() {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[osContext->getEngineType().type];

    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::submitLRCA(const MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(osContext->getEngineType()).mmioBase;
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), 0);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), 0);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), contextDescriptor.ulData[1]);
    stream->writeMMIO(AubMemDump::computeRegisterOffset(mmioBase, 0x2230), contextDescriptor.ulData[0]);
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::setupContext(OsContext &osContext) {
    CommandStreamReceiverHw<GfxFamily>::setupContext(osContext);

    engineIndex = getEngineIndex(osContext.getEngineType());
    auto &engineType = osContext.getEngineType();
    if (aubManager && !(engineType.type == lowPriorityGpgpuEngine.type && engineType.id == lowPriorityGpgpuEngine.id)) {
        hardwareContext = std::unique_ptr<aub_stream::HardwareContext>(aubManager->createHardwareContext(deviceIndex,
                                                                                                         engineIndex));
    }
}
} // namespace OCLRT
