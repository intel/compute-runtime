/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_tgllp_and_later.inl"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed, NEO::StreamProperties *streamProperties) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    NEO::Device *neoDevice = device->getNEODevice();
    auto csr = this->getCsr();
    bool isRcs = csr->isRcs();
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();

    bool useGlobalSshAndDsh = false;
    bool isDebuggerActive = neoDevice->getDebugger() != nullptr;

    uint64_t globalHeapsBase = 0;
    uint64_t indirectObjectHeapBaseAddress = 0;
    uint64_t bindlessSurfStateBase = 0ull;

    if (neoDevice->getBindlessHeapsHelper()) {
        useGlobalSshAndDsh = true;
        globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();

        if (!neoDevice->getBindlessHeapsHelper()->isGlobalDshSupported()) {
            bindlessSurfStateBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();
        }
    }

    NEO::StateBaseAddressProperties *sbaProperties = nullptr;

    if (streamProperties != nullptr) {
        sbaProperties = &streamProperties->stateBaseAddress;
    }

    uint64_t instructionHeapBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(neoDevice->getRootDeviceIndex()));

    STATE_BASE_ADDRESS sbaCmd;

    NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, rootDeviceEnvironment, isRcs, csr->getDcFlushSupport());
    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStream, {}, true, rootDeviceEnvironment, isRcs);
    auto l1CachePolicyData = csr->getStoredL1CachePolicy();

    NEO::StateBaseAddressHelperArgs<GfxFamily> stateBaseAddressHelperArgs = {
        gsba,                                             // generalStateBaseAddress
        indirectObjectHeapBaseAddress,                    // indirectObjectHeapBaseAddress
        instructionHeapBaseAddress,                       // instructionHeapBaseAddress
        globalHeapsBase,                                  // globalHeapsBaseAddress
        0,                                                // surfaceStateBaseAddress
        bindlessSurfStateBase,                            // bindlessSurfaceStateBaseAddress
        &sbaCmd,                                          // stateBaseAddressCmd
        sbaProperties,                                    // sbaProperties
        nullptr,                                          // dsh
        nullptr,                                          // ioh
        nullptr,                                          // ssh
        neoDevice->getGmmHelper(),                        // gmmHelper
        (device->getMOCS(cachedMOCSAllowed, false) >> 1), // statelessMocsIndex
        l1CachePolicyData->getL1CacheValue(false),        // l1CachePolicy
        l1CachePolicyData->getL1CacheValue(true),         // l1CachePolicyDebuggerActive
        NEO::MemoryCompressionState::notApplicable,       // memoryCompressionState
        true,                                             // setInstructionStateBaseAddress
        true,                                             // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                               // useGlobalHeapsBaseAddress
        false,                                            // isMultiOsContextCapable
        false,                                            // areMultipleSubDevicesInContext
        false,                                            // overrideSurfaceStateBaseAddress
        isDebuggerActive,                                 // isDebuggerActive
        this->doubleSbaWa,                                // doubleSbaWa
        this->heaplessModeEnabled                         // this->heaplessModeEnabled
    };

    NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs, commandStream);

    bool sbaTrackingEnabled = (NEO::Debugger::isDebugEnabled(this->internalUsage) && device->getL0Debugger());
    NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled, *neoDevice, commandStream, sbaCmd, true);

    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStream, {}, false, rootDeviceEnvironment, isRcs);

    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdDispatchSize(bool bindingTableBaseAddress) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    size_t size = sizeof(STATE_BASE_ADDRESS) + NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() +
                  NEO::EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(*device->getNEODevice(), this->csr->isRcs());

    size += estimateStateBaseAddressDebugTracking();
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    return estimateStateBaseAddressCmdDispatchSize(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpace(NEO::HeapContainer &heapContainer,
                                                       NEO::ScratchSpaceController *scratchController,
                                                       NEO::GraphicsAllocation *globalStatelessAllocation,
                                                       bool &gsbaState, bool &frontEndState,
                                                       uint32_t perThreadScratchSpaceSlot0Size, uint32_t perThreadScratchSpaceSlot1Size) {

    if (perThreadScratchSpaceSlot0Size > 0) {
        scratchController->setRequiredScratchSpace(nullptr, 0u, perThreadScratchSpaceSlot0Size, 0u,
                                                   csr->getOsContext(), gsbaState, frontEndState);
        auto scratchAllocation = scratchController->getScratchSpaceSlot0Allocation();
        csr->makeResident(*scratchAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::patchCommands(CommandList &commandList, uint64_t scratchAddress,
                                                  bool patchNewScratchController) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto &commandsToPatch = commandList.getCommandsToPatch();
    for (auto &commandToPatch : commandsToPatch) {
        switch (commandToPatch.type) {
        case CommandToPatch::FrontEndState: {
            UNRECOVERABLE_IF(true);
            break;
        }
        case CommandToPatch::PauseOnEnqueueSemaphoreStart: {
            NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserStartConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false, true, false, false, false);
            break;
        }
        case CommandToPatch::PauseOnEnqueueSemaphoreEnd: {
            NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserEndConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false, true, false, false, false);
            break;
        }
        case CommandToPatch::PauseOnEnqueuePipeControlStart: {

            NEO::PipeControlArgs args;
            args.dcFlushEnable = csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::immediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserStartConfirmation),
                device->getNEODevice()->getRootDeviceEnvironment(),
                args);
            break;
        }
        case CommandToPatch::PauseOnEnqueuePipeControlEnd: {

            NEO::PipeControlArgs args;
            args.dcFlushEnable = csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::immediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserEndConfirmation),
                device->getNEODevice()->getRootDeviceEnvironment(),
                args);
            break;
        }
        case CommandToPatch::NoopSpace: {
            memset(commandToPatch.pDestination, 0, commandToPatch.patchSize);
            break;
        }
        default: {
            UNRECOVERABLE_IF(true);
        }
        }
    }
}

} // namespace L0
