/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"

#include "igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed, NEO::StreamProperties *streamProperties) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t rootDeviceIndex = neoDevice->getRootDeviceIndex();

    auto csr = this->getCsr();
    bool dispatchCommand = false;
    bool multiOsContextCapable = device->isImplicitScalingCapable();
    bool isRcs = csr->isRcs();
    auto isDebuggerActive = neoDevice->isDebuggerActive() || neoDevice->getDebugger() != nullptr;
    bool setGeneralStateBaseAddress = false;
    bool useGlobalHeapsBaseAddress = false;

    uint64_t globalHeapsBase = 0;
    uint64_t indirectObjectStateBaseAddress = 0;
    uint64_t instructionStateBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(
        rootDeviceIndex, neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));

    NEO::StateBaseAddressProperties *sbaProperties = nullptr;

    auto l1CachePolicyData = csr->getStoredL1CachePolicy();

    if (streamProperties != nullptr) {
        dispatchCommand = true;
        sbaProperties = &streamProperties->stateBaseAddress;
    } else {
        if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
            globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();
            indirectObjectStateBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, useLocalMemoryForIndirectHeap);

            dispatchCommand = true;
            setGeneralStateBaseAddress = true;
            useGlobalHeapsBaseAddress = true;
        }
    }

    if (dispatchCommand) {
        auto gmmHelper = neoDevice->getGmmHelper();
        NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, neoDevice->getRootDeviceEnvironment(), isRcs, csr->getDcFlushSupport());

        STATE_BASE_ADDRESS sbaCmd;
        NEO::StateBaseAddressHelperArgs<GfxFamily> stateBaseAddressHelperArgs = {
            0,                                                // generalStateBaseAddress
            indirectObjectStateBaseAddress,                   // indirectObjectHeapBaseAddress
            instructionStateBaseAddress,                      // instructionHeapBaseAddress
            globalHeapsBase,                                  // globalHeapsBaseAddress
            0,                                                // surfaceStateBaseAddress
            &sbaCmd,                                          // stateBaseAddressCmd
            sbaProperties,                                    // sbaProperties
            nullptr,                                          // dsh
            nullptr,                                          // ioh
            nullptr,                                          // ssh
            gmmHelper,                                        // gmmHelper
            (device->getMOCS(cachedMOCSAllowed, false) >> 1), // statelessMocsIndex
            l1CachePolicyData->getL1CacheValue(false),        // l1CachePolicy
            l1CachePolicyData->getL1CacheValue(true),         // l1CachePolicyDebuggerActive
            NEO::MemoryCompressionState::NotApplicable,       // memoryCompressionState
            true,                                             // setInstructionStateBaseAddress
            setGeneralStateBaseAddress,                       // setGeneralStateBaseAddress
            useGlobalHeapsBaseAddress,                        // useGlobalHeapsBaseAddress
            multiOsContextCapable,                            // isMultiOsContextCapable
            false,                                            // useGlobalAtomics
            false,                                            // areMultipleSubDevicesInContext
            false,                                            // overrideSurfaceStateBaseAddress
            isDebuggerActive,                                 // isDebuggerActive
            this->doubleSbaWa                                 // doubleSbaWa
        };
        NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs, commandStream);

        bool sbaTrackingEnabled = (NEO::Debugger::isDebugEnabled(this->internalUsage) && device->getL0Debugger());
        NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                     *neoDevice,
                                                                                     commandStream,
                                                                                     sbaCmd, true);

        if (sbaProperties) {
            if (sbaProperties->bindingTablePoolBaseAddress.value != NEO::StreamProperty64::initValue) {
                NEO::StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(
                    commandStream,
                    static_cast<uint64_t>(sbaProperties->bindingTablePoolBaseAddress.value),
                    static_cast<uint32_t>(sbaProperties->bindingTablePoolSize.value),
                    gmmHelper);
            }
        } else {
            auto heap = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::GLOBAL_SSH);
            NEO::StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(
                commandStream,
                *heap,
                gmmHelper);
        }
    }

    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdDispatchSize(bool bindingTableBaseAddress) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    size_t size = sizeof(STATE_BASE_ADDRESS) + NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);

    if (this->doubleSbaWa) {
        size += sizeof(STATE_BASE_ADDRESS);
    }
    if (bindingTableBaseAddress) {
        size += sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC);
    }
    size += estimateStateBaseAddressDebugTracking();
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        return estimateStateBaseAddressCmdDispatchSize(true);
    }
    return 0;
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
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto &commandsToPatch = commandList.getCommandsToPatch();
    for (auto &commandToPatch : commandsToPatch) {
        switch (commandToPatch.type) {
        case CommandList::CommandToPatch::FrontEndState: {
            UNRECOVERABLE_IF(scratchAddress == 0u);
            uint32_t lowScratchAddress = uint32_t(0xFFFFFFFF & scratchAddress);
            CFE_STATE *cfeStateCmd = nullptr;
            cfeStateCmd = reinterpret_cast<CFE_STATE *>(commandToPatch.pCommand);

            cfeStateCmd->setScratchSpaceBuffer(lowScratchAddress);
            cfeStateCmd->setSingleSliceDispatchCcsMode(csr->getOsContext().isEngineInstanced());

            *reinterpret_cast<CFE_STATE *>(commandToPatch.pDestination) = *cfeStateCmd;
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreStart: {
            NEO::EncodeSempahore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserStartConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false, true);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreEnd: {
            NEO::EncodeSempahore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserEndConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false, true);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlStart: {

            NEO::PipeControlArgs args;
            args.dcFlushEnable = csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::ImmediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserStartConfirmation),
                device->getNEODevice()->getRootDeviceEnvironment(),
                args);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlEnd: {

            NEO::PipeControlArgs args;
            args.dcFlushEnable = csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::ImmediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserEndConfirmation),
                device->getNEODevice()->getRootDeviceEnvironment(),
                args);
            break;
        }
        default:
            UNRECOVERABLE_IF(true);
        }
    }
}

} // namespace L0
