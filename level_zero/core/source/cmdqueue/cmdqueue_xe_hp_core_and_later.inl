/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"

#include "igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        NEO::Device *neoDevice = device->getNEODevice();
        auto globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();
        auto &hwInfo = neoDevice->getHardwareInfo();
        bool isRcs = this->getCsr()->isRcs();

        NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, hwInfo, isRcs);
        auto pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
        STATE_BASE_ADDRESS sbaCmd;
        bool multiOsContextCapable = device->isImplicitScalingCapable();
        NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(&sbaCmd,
                                                                        nullptr,
                                                                        nullptr,
                                                                        nullptr,
                                                                        0,
                                                                        true,
                                                                        (device->getMOCS(cachedMOCSAllowed, false) >> 1),
                                                                        neoDevice->getMemoryManager()->getInternalHeapBaseAddress(neoDevice->getRootDeviceIndex(), useLocalMemoryForIndirectHeap),
                                                                        neoDevice->getMemoryManager()->getInternalHeapBaseAddress(neoDevice->getRootDeviceIndex(), neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(neoDevice->getRootDeviceIndex())),
                                                                        globalHeapsBase,
                                                                        true,
                                                                        true,
                                                                        neoDevice->getGmmHelper(),
                                                                        multiOsContextCapable,
                                                                        NEO::MemoryCompressionState::NotApplicable,
                                                                        false,
                                                                        1u);
        *pSbaCmd = sbaCmd;

        auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
        if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
            *pSbaCmd = sbaCmd;
        }

        if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {

            NEO::Debugger::SbaAddresses sbaAddresses = {};
            sbaAddresses.BindlessSurfaceStateBaseAddress = sbaCmd.getBindlessSurfaceStateBaseAddress();
            sbaAddresses.DynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
            sbaAddresses.GeneralStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
            sbaAddresses.InstructionBaseAddress = sbaCmd.getInstructionBaseAddress();
            sbaAddresses.SurfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();

            device->getL0Debugger()->programSbaTrackingCommands(commandStream, sbaAddresses);
        }

        auto heap = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::GLOBAL_SSH);
        auto cmd = GfxFamily::cmdInitStateBindingTablePoolAlloc;
        cmd.setBindingTablePoolBaseAddress(heap->getHeapGpuBase());
        cmd.setBindingTablePoolBufferSize(heap->getHeapSizeInPages());
        cmd.setSurfaceObjectControlStateIndexToMocsTables(neoDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));

        auto buffer = commandStream.getSpace(sizeof(cmd));
        *(typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC *)buffer = cmd;
    }
    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    NEO::Device *neoDevice = device->getNEODevice();
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    size_t size = 0;

    if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        size += sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL) + sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC);

        if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            size += sizeof(STATE_BASE_ADDRESS);
        }
    }

    return size;
}

constexpr uint32_t maxPtssIndex = 15u;

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpace(NEO::HeapContainer &sshHeaps,
                                                       NEO::ScratchSpaceController *scratchController,
                                                       bool &gsbaState, bool &frontEndState,
                                                       uint32_t perThreadScratchSpaceSize, uint32_t perThreadPrivateScratchSize) {
    if (perThreadScratchSpaceSize > 0 || perThreadPrivateScratchSize > 0) {
        if (sshHeaps.size() > 0) {
            uint32_t offsetIndex = maxPtssIndex * csr->getOsContext().getEngineType() + 1u;
            scratchController->programHeaps(sshHeaps, offsetIndex, perThreadScratchSpaceSize, perThreadPrivateScratchSize, csr->peekTaskCount(),
                                            csr->getOsContext(), gsbaState, frontEndState);
        }
        if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
            scratchController->programBindlessSurfaceStateForScratch(device->getNEODevice()->getBindlessHeapsHelper(), perThreadScratchSpaceSize, perThreadPrivateScratchSize, csr->peekTaskCount(),
                                                                     csr->getOsContext(), gsbaState, frontEndState, csr);
        }
        auto scratchAllocation = scratchController->getScratchSpaceAllocation();
        if (scratchAllocation != nullptr) {
            csr->makeResident(*scratchAllocation);
        }

        auto privateScratchAllocation = scratchController->getPrivateScratchSpaceAllocation();

        if (privateScratchAllocation != nullptr) {
            csr->makeResident(*privateScratchAllocation);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::patchCommands(CommandList &commandList, uint64_t scratchAddress) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using CFE_STATE = typename GfxFamily::CFE_STATE;

    uint32_t lowScratchAddress = uint32_t(0xFFFFFFFF & scratchAddress);

    CFE_STATE *cfeStateCmd = nullptr;

    auto &commandsToPatch = commandList.getCommandsToPatch();
    for (auto &commandToPatch : commandsToPatch) {
        switch (commandToPatch.type) {
        case CommandList::CommandToPatch::FrontEndState:
            cfeStateCmd = reinterpret_cast<CFE_STATE *>(commandToPatch.pCommand);

            cfeStateCmd->setScratchSpaceBuffer(lowScratchAddress);
            cfeStateCmd->setSingleSliceDispatchCcsMode(csr->getOsContext().isEngineInstanced());

            *reinterpret_cast<CFE_STATE *>(commandToPatch.pDestination) = *cfeStateCmd;
            break;
        default:
            UNRECOVERABLE_IF(true);
        }
    }
}

} // namespace L0
