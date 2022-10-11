/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <limits>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    const auto &hwInfo = this->device->getHwInfo();
    NEO::Device *neoDevice = device->getNEODevice();
    bool isRcs = this->getCsr()->isRcs();

    NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, hwInfo, isRcs, this->getCsr()->getDcFlushSupport());
    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStream, {}, true, hwInfo, isRcs);

    STATE_BASE_ADDRESS sbaCmd;

    bool useGlobalSshAndDsh = NEO::ApiSpecificConfig::getBindlessConfiguration();
    uint64_t globalHeapsBase = 0;
    if (useGlobalSshAndDsh) {
        globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();
    }

    auto indirectObjectHeapBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), useLocalMemoryForIndirectHeap);
    auto instructionHeapBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(neoDevice->getRootDeviceIndex()));
    auto isDebuggerActive = neoDevice->isDebuggerActive() || neoDevice->getDebugger() != nullptr;

    NEO::StateBaseAddressHelperArgs<GfxFamily> stateBaseAddressHelperArgs = {
        gsba,                                             // generalStateBase
        indirectObjectHeapBaseAddress,                    // indirectObjectHeapBaseAddress
        instructionHeapBaseAddress,                       // instructionHeapBaseAddress
        globalHeapsBase,                                  // globalHeapsBaseAddress
        0,                                                // surfaceStateBaseAddress
        &sbaCmd,                                          // stateBaseAddressCmd
        nullptr,                                          // dsh
        nullptr,                                          // ioh
        nullptr,                                          // ssh
        neoDevice->getGmmHelper(),                        // gmmHelper
        &hwInfo,                                          // hwInfo
        (device->getMOCS(cachedMOCSAllowed, false) >> 1), // statelessMocsIndex
        NEO::MemoryCompressionState::NotApplicable,       // memoryCompressionState
        true,                                             // setInstructionStateBaseAddress
        true,                                             // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                               // useGlobalHeapsBaseAddress
        false,                                            // isMultiOsContextCapable
        false,                                            // useGlobalAtomics
        false,                                            // areMultipleSubDevicesInContext
        false,                                            // overrideSurfaceStateBaseAddress
        isDebuggerActive                                  // isDebuggerActive
    };

    NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs, commandStream);

    bool sbaTrackingEnabled = (NEO::Debugger::isDebugEnabled(this->internalUsage) && device->getL0Debugger());
    NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled, *neoDevice, commandStream, sbaCmd);

    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStream, {}, false, hwInfo, isRcs);

    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    size_t size = sizeof(STATE_BASE_ADDRESS) + NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false) +
                  NEO::EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(*device->getNEODevice(), this->csr->isRcs());

    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger() != nullptr) {
        const size_t trackedAddressesCount = 6;
        size += device->getL0Debugger()->getSbaTrackingCommandsSize(trackedAddressesCount);
    }
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpace(NEO::HeapContainer &heapContainer,
                                                       NEO::ScratchSpaceController *scratchController,
                                                       bool &gsbaState, bool &frontEndState,
                                                       uint32_t perThreadScratchSpaceSize, uint32_t perThreadPrivateScratchSize) {

    if (perThreadScratchSpaceSize > 0) {
        scratchController->setRequiredScratchSpace(nullptr, 0u, perThreadScratchSpaceSize, 0u, csr->peekTaskCount(),
                                                   csr->getOsContext(), gsbaState, frontEndState);
        auto scratchAllocation = scratchController->getScratchSpaceAllocation();
        csr->makeResident(*scratchAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::patchCommands(CommandList &commandList, uint64_t scratchAddress) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto &commandsToPatch = commandList.getCommandsToPatch();
    for (auto &commandToPatch : commandsToPatch) {
        switch (commandToPatch.type) {
        case CommandList::CommandToPatch::FrontEndState: {
            UNRECOVERABLE_IF(true);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreStart: {
            NEO::EncodeSempahore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserStartConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreEnd: {
            NEO::EncodeSempahore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandToPatch.pCommand),
                                                                    csr->getDebugPauseStateGPUAddress(),
                                                                    static_cast<uint32_t>(NEO::DebugPauseState::hasUserEndConfirmation),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                    false);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlStart: {
            auto &hwInfo = device->getNEODevice()->getHardwareInfo();

            NEO::PipeControlArgs args;
            args.dcFlushEnable = this->csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::ImmediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserStartConfirmation),
                hwInfo,
                args);
            break;
        }
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlEnd: {
            auto &hwInfo = device->getNEODevice()->getHardwareInfo();

            NEO::PipeControlArgs args;
            args.dcFlushEnable = this->csr->getDcFlushSupport();

            auto command = reinterpret_cast<void *>(commandToPatch.pCommand);
            NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                command,
                NEO::PostSyncMode::ImmediateData,
                csr->getDebugPauseStateGPUAddress(),
                static_cast<uint64_t>(NEO::DebugPauseState::waitingForUserEndConfirmation),
                hwInfo,
                args);
            break;
        }
        default: {
            UNRECOVERABLE_IF(true);
        }
        }
    }
}

} // namespace L0
