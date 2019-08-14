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
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"

#include "aub_mapper.h"
#include "third_party/aub_stream/headers/aub_manager.h"

namespace NEO {

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
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::setupContext(OsContext &osContext) {
    CommandStreamReceiverHw<GfxFamily>::setupContext(osContext);

    auto engineType = osContext.getEngineType();
    uint32_t flags = 0;
    getCsTraits(engineType).setContextSaveRestoreFlags(flags);

    if (aubManager && !osContext.isLowPriority()) {
        hardwareContextController = std::make_unique<HardwareContextController>(*aubManager, osContext, flags);
    }
}

template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getParametersForWriteMemory(GraphicsAllocation &graphicsAllocation, uint64_t &gpuAddress, void *&cpuAddress, size_t &size) const {
    cpuAddress = graphicsAllocation.getUnderlyingBuffer();
    gpuAddress = GmmHelper::decanonize(graphicsAllocation.getGpuAddress());
    size = graphicsAllocation.getUnderlyingBufferSize();
    auto gmm = graphicsAllocation.getDefaultGmm();
    if (gmm && gmm->isRenderCompressed) {
        size = gmm->gmmResourceInfo->getSizeAllocation();
    }

    if (size == 0)
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

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::freeEngineInfo(AddressMapper &gttRemap) {
    alignedFree(engineInfo.pLRCA);
    gttRemap.unmap(engineInfo.pLRCA);
    engineInfo.pLRCA = nullptr;

    alignedFree(engineInfo.pGlobalHWStatusPage);
    gttRemap.unmap(engineInfo.pGlobalHWStatusPage);
    engineInfo.pGlobalHWStatusPage = nullptr;

    alignedFree(engineInfo.pRingBuffer);
    gttRemap.unmap(engineInfo.pRingBuffer);
    engineInfo.pRingBuffer = nullptr;
}

} // namespace NEO
