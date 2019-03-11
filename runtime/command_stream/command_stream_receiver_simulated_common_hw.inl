/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"

#include "aub_mapper.h"
#include "third_party/aub_stream/headers/aub_manager.h"

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

    auto engineType = osContext.getEngineType();
    engineIndex = getEngineIndex(engineType);
    uint32_t flags = 0;
    getCsTraits(engineType).setContextSaveRestoreFlags(flags);

    if (aubManager && !(engineType.type == lowPriorityGpgpuEngine.type && engineType.id == lowPriorityGpgpuEngine.id)) {
        hardwareContextController = std::make_unique<HardwareContextController>(*aubManager, osContext, engineIndex, flags);
    }
}

template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getParametersForWriteMemory(GraphicsAllocation &graphicsAllocation, uint64_t &gpuAddress, void *&cpuAddress, size_t &size) const {
    cpuAddress = ptrOffset(graphicsAllocation.getUnderlyingBuffer(), static_cast<size_t>(graphicsAllocation.getAllocationOffset()));
    gpuAddress = GmmHelper::decanonize(graphicsAllocation.getGpuAddress());
    size = graphicsAllocation.getUnderlyingBufferSize();
    if (graphicsAllocation.gmm && graphicsAllocation.gmm->isRenderCompressed) {
        size = graphicsAllocation.gmm->gmmResourceInfo->getSizeAllocation();
    }

    if ((size == 0) || !graphicsAllocation.isAubWritable())
        return false;

    if (cpuAddress == nullptr) {
        cpuAddress = this->getMemoryManager()->lockResource(&graphicsAllocation);
    }
    return true;
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    this->expectMemory(gfxAddress, srcAddress, length,
                       AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);
}
template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    this->expectMemory(gfxAddress, srcAddress, length,
                       AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);
}

} // namespace OCLRT
