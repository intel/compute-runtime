/*
 * Copyright (C) 2021-2026 Intel Corporation
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
#include "shared/source/host_function/host_function.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"

#include "neo_igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream, bool cachedMOCSAllowed, NEO::StreamProperties *streamProperties) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t rootDeviceIndex = neoDevice->getRootDeviceIndex();

    auto csr = this->getCsr();

    if (streamProperties != nullptr) {
        bool multiOsContextCapable = device->isImplicitScalingCapable();
        bool isRcs = csr->isRcs();
        auto isDebuggerActive = neoDevice->getDebugger() != nullptr;
        bool setGeneralStateBaseAddress = false;
        bool useGlobalSshAndDsh = false;

        uint64_t globalHeapsBase = 0;
        uint64_t indirectObjectStateBaseAddress = 0;
        uint64_t instructionStateBaseAddress = neoDevice->getMemoryManager()->getInternalHeapBaseAddress(
            rootDeviceIndex, neoDevice->getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));
        uint64_t bindlessSurfStateBase = 0;

        if (device->getNEODevice()->getBindlessHeapsHelper()) {
            useGlobalSshAndDsh = true;
            globalHeapsBase = neoDevice->getBindlessHeapsHelper()->getGlobalHeapsBase();

            if (!device->getNEODevice()->getBindlessHeapsHelper()->isGlobalDshSupported()) {
                bindlessSurfStateBase = device->getNEODevice()->getBindlessHeapsHelper()->getGlobalHeapsBase();
            }
        }

        NEO::StateBaseAddressProperties *sbaProperties = nullptr;

        auto l1CachePolicyData = csr->getStoredL1CachePolicy();

        sbaProperties = &streamProperties->stateBaseAddress;

        auto gmmHelper = neoDevice->getGmmHelper();
        NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStream, neoDevice->getRootDeviceEnvironment(), isRcs, csr->getDcFlushSupport());

        STATE_BASE_ADDRESS sbaCmd;
        NEO::StateBaseAddressHelperArgs<GfxFamily> stateBaseAddressHelperArgs = {
            0,                                                // generalStateBaseAddress
            indirectObjectStateBaseAddress,                   // indirectObjectHeapBaseAddress
            instructionStateBaseAddress,                      // instructionHeapBaseAddress
            globalHeapsBase,                                  // globalHeapsBaseAddress
            0,                                                // surfaceStateBaseAddress
            bindlessSurfStateBase,                            // bindlessSurfaceStateBaseAddress
            &sbaCmd,                                          // stateBaseAddressCmd
            sbaProperties,                                    // sbaProperties
            nullptr,                                          // dsh
            nullptr,                                          // ioh
            nullptr,                                          // ssh
            gmmHelper,                                        // gmmHelper
            (device->getMOCS(cachedMOCSAllowed, false) >> 1), // statelessMocsIndex
            l1CachePolicyData->getL1CacheValue(false),        // l1CachePolicy
            l1CachePolicyData->getL1CacheValue(true),         // l1CachePolicyDebuggerActive
            NEO::MemoryCompressionState::notApplicable,       // memoryCompressionState
            true,                                             // setInstructionStateBaseAddress
            setGeneralStateBaseAddress,                       // setGeneralStateBaseAddress
            useGlobalSshAndDsh,                               // useGlobalHeapsBaseAddress
            multiOsContextCapable,                            // isMultiOsContextCapable
            false,                                            // areMultipleSubDevicesInContext
            false,                                            // overrideSurfaceStateBaseAddress
            isDebuggerActive,                                 // isDebuggerActive
            this->doubleSbaWa,                                // doubleSbaWa
            this->heaplessModeEnabled                         // heaplessModeEnabled
        };
        NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs, commandStream);

        bool sbaTrackingEnabled = (NEO::Debugger::isDebugEnabled(this->internalUsage) && device->getL0Debugger());
        NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                     *neoDevice,
                                                                                     commandStream,
                                                                                     sbaCmd, true);

        if (sbaProperties->bindingTablePoolBaseAddress.value != NEO::StreamProperty64::initValue) {
            NEO::StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(
                commandStream,
                static_cast<uint64_t>(sbaProperties->bindingTablePoolBaseAddress.value),
                static_cast<uint32_t>(sbaProperties->bindingTablePoolSize.value),
                gmmHelper);
        }
    }

    csr->setGSBAStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdDispatchSize(bool bindingTableBaseAddress) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    size_t size = sizeof(STATE_BASE_ADDRESS) + NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();

    if (this->doubleSbaWa) {
        size += sizeof(STATE_BASE_ADDRESS);
    }
    if (bindingTableBaseAddress) {
        if constexpr (!GfxFamily::isHeaplessRequired()) {
            using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
            size += sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC);
        }
    }
    size += estimateStateBaseAddressDebugTracking();
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpace(NEO::HeapContainer &sshHeaps,
                                                       NEO::ScratchSpaceController *scratchController,
                                                       NEO::GraphicsAllocation *globalStatelessAllocation,
                                                       bool &gsbaState, bool &frontEndState,
                                                       uint32_t perThreadScratchSpaceSlot0Size, uint32_t perThreadScratchSpaceSlot1Size) {
    if (perThreadScratchSpaceSlot0Size > 0 || perThreadScratchSpaceSlot1Size > 0) {
        if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
            scratchController->setRequiredScratchSpace(globalStatelessAllocation->getUnderlyingBuffer(), 0, perThreadScratchSpaceSlot0Size, perThreadScratchSpaceSlot1Size,
                                                       csr->getOsContext(), gsbaState, frontEndState);
        }

        if (sshHeaps.size() > 0) {
            scratchController->programHeaps(sshHeaps, 1u, perThreadScratchSpaceSlot0Size, perThreadScratchSpaceSlot1Size,
                                            csr->getOsContext(), gsbaState, frontEndState);
        }

        auto scratch0Allocation = scratchController->getScratchSpaceSlot0Allocation();
        if (scratch0Allocation != nullptr) {
            csr->makeResident(*scratch0Allocation);
        }

        auto scratch1Allocation = scratchController->getScratchSpaceSlot1Allocation();

        if (scratch1Allocation != nullptr) {
            csr->makeResident(*scratch1Allocation);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchFrontEndState &patchElem) {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        using CFE_STATE = typename GfxFamily::CFE_STATE;
        uint32_t lowScratchAddress = uint32_t(0xFFFFFFFF & scratchAddress);
        CFE_STATE *cfeStateCmd = nullptr;
        cfeStateCmd = reinterpret_cast<CFE_STATE *>(patchElem.pCommand);

        cfeStateCmd->setScratchSpaceBuffer(lowScratchAddress);
        NEO::PreambleHelper<GfxFamily>::setSingleSliceDispatchMode(cfeStateCmd, false);

        if (patchPreambleEnabled) {
            NEO::EncodeDataMemory<GfxFamily>::programDataMemory(*patchPreambleBuffer, patchElem.gpuAddress, patchElem.pCommand, sizeof(CFE_STATE));
        } else {
            *reinterpret_cast<CFE_STATE *>(patchElem.pDestination) = *cfeStateCmd;
        }
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchComputeWalkerInlineDataScratch &patchElem) {
    if (NEO::isUndefined(patchElem.patchSize) || NEO::isUndefinedOffset(patchElem.offset)) {
        return;
    }
    if (!patchNewScratchController && patchElem.scratchAddressAfterPatch == scratchAddress) {
        return;
    }

    uint64_t fullScratchAddress = scratchAddress + patchElem.baseAddress;
    if (patchPreambleEnabled) {
        uint64_t gpuAddressToPatch = patchElem.gpuAddress + patchElem.offset;
        NEO::EncodeDataMemory<GfxFamily>::programDataMemory(*patchPreambleBuffer, gpuAddressToPatch, &fullScratchAddress, patchElem.patchSize);
    } else {
        void *scratchAddressPatch = ptrOffset(patchElem.pDestination, patchElem.offset);
        std::memcpy(scratchAddressPatch, &fullScratchAddress, patchElem.patchSize);
    }
    patchElem.scratchAddressAfterPatch = scratchAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchComputeWalkerImplicitArgsScratch &patchElem) {
    if (!patchNewScratchController && patchElem.scratchAddressAfterPatch == scratchAddress) {
        return;
    }
    uint64_t fullScratchAddress = scratchAddress + patchElem.baseAddress;
    if (patchPreambleEnabled) {
        uint64_t gpuAddressToPatch = patchElem.gpuAddress + patchElem.offset;
        NEO::EncodeDataMemory<GfxFamily>::programDataMemory(*patchPreambleBuffer, gpuAddressToPatch, &fullScratchAddress, patchElem.patchSize);
    } else {
        void *scratchAddressPatch = ptrOffset(patchElem.pDestination, patchElem.offset);
        std::memcpy(scratchAddressPatch, &fullScratchAddress, patchElem.patchSize);
    }
    patchElem.scratchAddressAfterPatch = scratchAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchNoopSpace &patchElem) {
    if (patchElem.pDestination != nullptr) {
        if (patchPreambleEnabled) {
            NEO::EncodeDataMemory<GfxFamily>::programNoop(*patchPreambleBuffer, patchElem.gpuAddress, patchElem.patchSize);
        } else {
            memset(patchElem.pDestination, 0, patchElem.patchSize);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchHostFunctionId &patchElem) {
    NEO::HostFunction hostFunction = {.hostFunctionAddress = patchElem.callbackAddress,
                                      .userDataAddress = patchElem.userDataAddress};

    auto allocator = queue.getDevice()->getHostFunctionAllocator(queue.csr);
    queue.csr->ensureHostFunctionWorkerStarted(allocator);
    auto &hostFunctionStreamer = queue.csr->getHostFunctionStreamer();

    if (patchPreambleEnabled) {
        auto size = NEO::HostFunctionHelper<GfxFamily>::getSizeForHostFunctionIdProgramming(memorySynchronizationRequired, queue.csr->getDcFlushSupport());
        auto cmdStorage = std::make_unique_for_overwrite<uint8_t[]>(size);
        void *cmdBuffer = static_cast<void *>(cmdStorage.get());

        NEO::HostFunctionHelper<GfxFamily>::programHostFunctionId(nullptr,
                                                                  cmdBuffer,
                                                                  hostFunctionStreamer,
                                                                  std::move(hostFunction),
                                                                  memorySynchronizationRequired);

        NEO::EncodeDataMemory<GfxFamily>::programDataMemory(*patchPreambleBuffer, patchElem.gpuAddress, cmdBuffer, size);
    } else {
        NEO::HostFunctionHelper<GfxFamily>::programHostFunctionId(nullptr,
                                                                  patchElem.cmdBufferSpace,
                                                                  hostFunctionStreamer,
                                                                  std::move(hostFunction),
                                                                  memorySynchronizationRequired);
    }

    hostFunctionsCounter++;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::CommandsToPatchVisitor::operator()(PatchHostFunctionWait &patchElem) {
    auto partitionId = patchElem.partitionId;

    if (patchPreambleEnabled) {
        using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

        MI_SEMAPHORE_WAIT miSemaphore{};
        NEO::HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(nullptr,
                                                                                 &miSemaphore,
                                                                                 queue.csr->getHostFunctionStreamer(),
                                                                                 partitionId);
        auto size = NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
        NEO::EncodeDataMemory<GfxFamily>::programDataMemory(*patchPreambleBuffer, patchElem.gpuAddress, &miSemaphore, size);
    } else {
        NEO::HostFunctionHelper<GfxFamily>::programHostFunctionWaitForCompletion(nullptr,
                                                                                 patchElem.cmdBufferSpace,
                                                                                 queue.csr->getHostFunctionStreamer(),
                                                                                 partitionId);
    }
}

} // namespace L0
