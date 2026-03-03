/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/host_function/host_function.h"

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
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchFrontEndState &patchElem) {
    UNRECOVERABLE_IF(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchComputeWalkerInlineDataScratch &patchElem) {
    UNRECOVERABLE_IF(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchComputeWalkerImplicitArgsScratch &patchElem) {
    UNRECOVERABLE_IF(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchNoopSpace &patchElem) {
    if (patchElem.pDestination != nullptr) {
        memset(patchElem.pDestination, 0, patchElem.patchSize);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchHostFunctionId &patchElem) {
    NEO::HostFunction hostFunction = {.hostFunctionAddress = patchElem.callbackAddress,
                                      .userDataAddress = patchElem.userDataAddress};
    auto allocator = queue.getDevice()->getHostFunctionAllocator(queue.csr);
    queue.csr->ensureHostFunctionWorkerStarted(allocator);

    NEO::HostFunctionHelper<GfxFamily>::programHostFunctionId(nullptr,
                                                              patchElem.cmdBufferSpace,
                                                              queue.csr->getHostFunctionStreamer(),
                                                              std::move(hostFunction),
                                                              memorySynchronizationRequired);

    hostFunctionsCounter++;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchHostFunctionWait &patchElem) {
    auto partitionId = patchElem.partitionId;

    NEO::HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(nullptr,
                                                                             patchElem.cmdBufferSpace,
                                                                             queue.csr->getHostFunctionStreamer(),
                                                                             partitionId);
}

} // namespace L0
