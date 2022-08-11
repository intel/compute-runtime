/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/scratch_space_controller.h"
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
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    NEO::Device *neoDevice = device->getNEODevice();
    auto &hwInfo = neoDevice->getHardwareInfo();
    uint32_t rootDeviceIndex = neoDevice->getRootDeviceIndex();

    bool multiOsContextCapable = device->isImplicitScalingCapable();

    uint64_t indirectObjectStateBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, useLocalMemoryForIndirectHeap);
    uint64_t instructionStateBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(
        rootDeviceIndex, neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));

    if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        auto globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();

        bool isRcs = this->getCsr()->isRcs();

        NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, hwInfo, isRcs);
        auto sbaCmdBuf = NEO::StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);

        STATE_BASE_ADDRESS sbaCmd;
        NEO::StateBaseAddressHelperArgs<GfxFamily> args = {
            0,                                                // generalStateBase
            indirectObjectStateBaseAddress,                   // indirectObjectHeapBaseAddress
            instructionStateBaseAddress,                      // instructionHeapBaseAddress
            globalHeapsBase,                                  // globalHeapsBaseAddress
            &sbaCmd,                                          // stateBaseAddressCmd
            nullptr,                                          // dsh
            nullptr,                                          // ioh
            nullptr,                                          // ssh
            neoDevice->getGmmHelper(),                        // gmmHelper
            (device->getMOCS(cachedMOCSAllowed, false) >> 1), // statelessMocsIndex
            NEO::MemoryCompressionState::NotApplicable,       // memoryCompressionState
            true,                                             // setInstructionStateBaseAddress
            true,                                             // setGeneralStateBaseAddress
            true,                                             // useGlobalHeapsBaseAddress
            multiOsContextCapable,                            // isMultiOsContextCapable
            false,                                            // useGlobalAtomics
            false                                             // areMultipleSubDevicesInContext
        };

        NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(args);
        *sbaCmdBuf = sbaCmd;

        auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
        if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            sbaCmdBuf = NEO::StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);
            *sbaCmdBuf = sbaCmd;
        }

        if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {

            NEO::Debugger::SbaAddresses sbaAddresses = {};
            NEO::EncodeStateBaseAddress<GfxFamily>::setSbaAddressesForDebugger(sbaAddresses, sbaCmd);

            device->getL0Debugger()->programSbaTrackingCommands(commandStream, sbaAddresses);
        }

        auto heap = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::GLOBAL_SSH);
        NEO::StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(
            commandStream,
            *heap,
            neoDevice->getGmmHelper());
    }
    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
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
