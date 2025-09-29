/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/const_stringref.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.h"
#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/mutable_cmdlist_hw_experimental.inl"

#include "encode_surface_state_args.h"
#include "implicit_args.h"

#include <cstddef>
#include <optional>

namespace L0 {
namespace MCL {

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateKernelMemoryPrefetch(const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, const CommandToPatch &cmdToPatch, uint64_t cmdId) {
    NEO::LinearStream cmdStream(cmdToPatch.pDestination, cmdToPatch.patchSize);

    CommandListCoreFamily<gfxCoreFamily>::prefetchKernelMemory(cmdStream, kernel, iohAllocation, cmdToPatch.offset, nullptr, cmdId);

    DEBUG_BREAK_IF(cmdStream.getAvailableSpace() != 0);
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint64_t MutableCommandListCoreFamily<gfxCoreFamily>::getPrefetchCmdId() const {
    return nextCommandId;
}

template <GFXCORE_FAMILY gfxCoreFamily>
MutableKernelGroup *MutableCommandListCoreFamily<gfxCoreFamily>::getKernelGroupForPrefetch(uint64_t cmdId) const {
    if (!this->nextAppendKernelMutable) {
        return nullptr;
    }

    auto &currentAppend = this->kernelMutations[(cmdId - 1)];
    return currentAppend.kernelGroup;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::ensureCmdBufferSpaceForPrefetch() {
    auto kernelGroup = getKernelGroupForPrefetch(getPrefetchCmdId());
    if (!kernelGroup) {
        return;
    }

    auto expectedSize = NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(kernelGroup->getMaxAppendIndirectHeapSize(), this->device->getNEODevice()->getRootDeviceEnvironment()) +
                        NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(kernelGroup->getMaxIsaSize(), this->device->getNEODevice()->getRootDeviceEnvironment());
    this->commandContainer.getCommandStream()->ensureContinuousSpace(expectedSize);
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint32_t MutableCommandListCoreFamily<gfxCoreFamily>::getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const {
    if (auto kernelGroup = getKernelGroupForPrefetch(getPrefetchCmdId())) {
        return kernelGroup->getMaxAppendIndirectHeapSize();
    }

    return kernel.getIndirectSize() + reserveExtraSpace;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::addKernelIsaMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint32_t isaPrefetchSizeLimit, uint64_t cmdId) {
    auto kernelGroup = getKernelGroupForPrefetch(cmdId);
    if (!kernelGroup) {
        return;
    }

    auto maxSize = std::min(isaPrefetchSizeLimit, kernelGroup->getMaxIsaSize());

    auto remainingSize = maxSize - kernel.getImmutableData()->getIsaSize();

    auto paddingSize = NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(remainingSize, this->device->getNEODevice()->getRootDeviceEnvironment());
    NEO::EncodeNoop<GfxFamily>::emitNoop(cmdStream, paddingSize);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::addKernelIndirectDataMemoryPrefetchPadding(NEO::LinearStream &cmdStream, const Kernel &kernel, uint64_t cmdId) {
    auto kernelGroup = getKernelGroupForPrefetch(cmdId);
    if (!kernelGroup) {
        return;
    }

    auto remainingSize = kernelGroup->getMaxAppendIndirectHeapSize() - kernel.getIndirectSize();

    auto paddingSize = NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(remainingSize, this->device->getNEODevice()->getRootDeviceEnvironment());
    NEO::EncodeNoop<GfxFamily>::emitNoop(cmdStream, paddingSize);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::initialize(Device *device, NEO::EngineGroupType engineGroupType, ze_command_list_flags_t flags) {
    constexpr size_t estimatedMutableAppendCount = 500;
    // base dispatch variables are group count/size and optionally global offset
    constexpr size_t estimatedDispatchVariablesCount = 2;
    constexpr size_t estimatedKernelArgumentPerAppendCount = 40;
    constexpr size_t estimatedAllocationsPerAppendCount = estimatedKernelArgumentPerAppendCount + 1; // at least 1 for the kernel ISA
    // this is for regular variables without kernel ISA mutation, which goes into mutable_kernel.cpp
    constexpr size_t estimatedVariablesPerAppendCount = estimatedKernelArgumentPerAppendCount + estimatedDispatchVariablesCount;
    // commit variables are group count/size and optionally last slm argument per append
    constexpr size_t estimatedCommitVariablesPerAppend = estimatedDispatchVariablesCount;
    constexpr size_t estimatedDifferentKernelUsed = 25;

    auto ret = CommandListCoreFamily<gfxCoreFamily>::initialize(device, engineGroupType, flags);
    CommandListCoreFamily<gfxCoreFamily>::allowCbWaitEventsNoopDispatch = true;
    this->maxPerThreadDataSize = static_cast<uint32_t>(device->getDeviceInfo().maxWorkGroupSize * 3 * sizeof(uint16_t));
    this->iohAlignment = NEO::EncodeDispatchKernel<GfxFamily>::getDefaultIOHAlignment();
    this->inlineDataSize = getInlineDataSize();

    // this is a unique ptr storage for all variables
    this->variableStorage.reserve(estimatedMutableAppendCount * estimatedVariablesPerAppendCount);
    // this is for dispatch variables that can be used to commit at close
    this->stageCommitVariables.reserve(estimatedMutableAppendCount * estimatedCommitVariablesPerAppend);
    // this is container for mutable residency allocations
    this->mutableAllocations.reserveSpace(estimatedMutableAppendCount * estimatedAllocationsPerAppendCount);
    // this is a unique ptr storage for mutable compute walker commands
    this->mutableWalkerCmds.reserve(estimatedMutableAppendCount * estimatedDifferentKernelUsed); // product of appends and possible kernels in kernel groups

    // this is a unique ptr storage for all kernel data used at any given append/dispatch (offsets, sizes, addresses)
    this->dispatches.reserve(estimatedMutableAppendCount);
    // number of mutation points, aggregate pointers to all objects stored as pointers in different other classes
    this->kernelMutations.reserve(estimatedMutableAppendCount);
    this->eventMutations.reserve(estimatedMutableAppendCount);
    // this is a unique ptr storage for all kernel groups created at mutation points
    this->mutableKernelGroups.reserve(estimatedMutableAppendCount);

    // this is a unique ptr storage for all kernel meta data - its ISA GPUVA, properties, etc.
    this->kernelData.reserve(estimatedDifferentKernelUsed);

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                                                                   const ze_group_count_t &threadGroupDimensions,
                                                                                   ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                                                   ze_event_handle_t *phWaitEvents,
                                                                                   CmdListKernelLaunchParams &launchParams) {
    MutableAppendLaunchKernelEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        if (kernelInstructionMutationEnabled(this->nextMutationFlags) && CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled()) {
            launchParams.outListCommands = &this->appendCmdsToPatch;
        }

        if ((this->nextMutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS) == ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS) {
            if (numWaitEvents > 0) {
                AppendEventMutation &currentAppend = this->eventMutations[(nextCommandId - 1)];

                currentAppend.waitEvents.reserve(numWaitEvents);
                mutableEventParams.waitEvents = true;
                bool omitWaitEventResidency = false;

                for (uint32_t i = 0; i < numWaitEvents; i++) {
                    WaitEventVariableDescriptor mutableWaitEventDesc = {};

                    Event *event = Event::fromHandle(phWaitEvents[i]);

                    Variable *variable = nullptr;
                    InterfaceVariableDescriptor varDesc = {};
                    getVariable(&varDesc, &variable);

                    variable->setAsWaitEvent(event);

                    mutableWaitEventDesc.event = event;
                    mutableWaitEventDesc.eventVariable = variable;
                    mutableWaitEventDesc.waitEventIndex = i;

                    if (CommandListImp::isInOrderExecutionEnabled() && event->isCounterBased()) {
                        mutableWaitEventDesc.waitEventPackets = event->getInOrderExecInfo()->getNumDevicePartitionsToWait();
                        if (!isCbEventBoundToCmdList(event)) {
                            omitWaitEventResidency = true;
                            auto deviceCounterAlloc = event->getInOrderExecInfo()->getDeviceCounterAllocation();
                            addToResidencyContainer(getDeviceCounterAllocForResidency(deviceCounterAlloc));
                        }
                    } else {
                        mutableWaitEventDesc.waitEventPackets = event->getPacketsToWait();
                    }
                    currentAppend.waitEvents.push_back(mutableWaitEventDesc);

                    NEO::GraphicsAllocation *eventPoolAlloc = event->getAllocation(this->device);
                    if (eventPoolAlloc) {
                        omitWaitEventResidency = true;
                        addToResidencyContainer(eventPoolAlloc);
                    }
                }
                launchParams.omitAddingWaitEventsResidency = omitWaitEventResidency;
                launchParams.outListCommands = &this->appendCmdsToPatch;
            }
        }

        Event *signalEvent = Event::fromHandle(hEvent);
        storeSignalEventVariable(mutableEventParams, launchParams, signalEvent);
    }

    auto retCode = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernelHandle, threadGroupDimensions, hEvent, numWaitEvents, phWaitEvents, launchParams);
    if (retCode != ZE_RESULT_SUCCESS) {
        return retCode;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.signalEvent) {
            auto &signalEventVariableDesc = this->eventMutations[(nextCommandId - 1)].signalEvent;

            auto kernelGroup = this->kernelMutations[(nextCommandId - 1)].kernelGroup;
            if (kernelGroup != nullptr) {
                kernelGroup->setSharedSignalVariable(signalEventVariableDesc.eventVariable);
            }

            MutableComputeWalker *walker = nullptr;
            MutablePipeControl *signalPipeControl = nullptr;
            if (mutableEventParams.counterBasedEvent) {
                // both TS and L3 flush events need additional clean Store Data Imm -> signal cmd (CW or PC or StoreRegMem) -> sync SemWait
                if (mutableEventParams.counterBasedTimestampEvent || mutableEventParams.l3FlushEvent) {
                    auto &eventVariableSemaphoreWaitList = signalEventVariableDesc.eventVariable->getSemWaitList();
                    auto &eventVariableStoreDataImmList = signalEventVariableDesc.eventVariable->getStoreDataImmList();

                    captureCounterBasedTimestampSignalEventCommands(signalEventVariableDesc,
                                                                    eventVariableSemaphoreWaitList, eventVariableStoreDataImmList);
                    walker = this->appendKernelMutableComputeWalker;
                }
                if (mutableEventParams.l3FlushEventTimestampSyncCmds) {
                    // L3 TS is signaled by StoreRegMem
                    auto &eventVariableStoreRegMemList = signalEventVariableDesc.eventVariable->getStoreRegMemList();
                    captureStandaloneTimestampSignalEventCommands(eventVariableStoreRegMemList);
                } else if (mutableEventParams.l3FlushEventSyncCmd) {
                    // L3 Immediate is signaled by PC
                    auto signalPipeControlPtr = std::make_unique<MutablePipeControlHw<GfxFamily>>(mutableEventParams.signalCmd.pDestination);
                    mutablePipeControlCmds.emplace_back(std::move(signalPipeControlPtr));
                    signalPipeControl = (*mutablePipeControlCmds.rbegin()).get();
                }

                if (mutableEventParams.inOrderIncrementEvent) {
                    walker = this->appendKernelMutableComputeWalker;
                }
            } else {
                if (mutableEventParams.eventInsideInOrder) {
                    auto &eventVariableSemaphoreWaitList = signalEventVariableDesc.eventVariable->getSemWaitList();
                    auto &eventVariableStoreDataImmList = signalEventVariableDesc.eventVariable->getStoreDataImmList();

                    captureCounterBasedTimestampSignalEventCommands(signalEventVariableDesc,
                                                                    eventVariableSemaphoreWaitList, eventVariableStoreDataImmList);
                }
                if (mutableEventParams.l3FlushEventTimestampSyncCmds) {
                    auto &eventVariableStoreRegMemList = signalEventVariableDesc.eventVariable->getStoreRegMemList();
                    captureStandaloneTimestampSignalEventCommands(eventVariableStoreRegMemList);
                } else if (mutableEventParams.l3FlushEventSyncCmd) {
                    auto signalPipeControlPtr = std::make_unique<MutablePipeControlHw<GfxFamily>>(mutableEventParams.signalCmd.pDestination);
                    mutablePipeControlCmds.emplace_back(std::move(signalPipeControlPtr));
                    signalPipeControl = (*mutablePipeControlCmds.rbegin()).get();
                } else {
                    walker = this->appendKernelMutableComputeWalker;
                }
            }
            signalEventVariableDesc.eventVariable->setAsSignalEvent(signalEventVariableDesc.event,
                                                                    walker,
                                                                    signalPipeControl);
            NEO::GraphicsAllocation *eventAlloc = signalEventVariableDesc.event->getAllocation(this->device);
            if (eventAlloc) {
                addToResidencyContainer(eventAlloc);
            }
        }
        if (mutableEventParams.waitEvents) {
            auto waitEventCmdToPatchIterator = this->appendCmdsToPatch.begin();
            if (waitEventCmdToPatchIterator->type == CommandToPatch::CommandType::PrefetchKernelMemory) {
                waitEventCmdToPatchIterator++;
            }

            AppendEventMutation &currentAppend = this->eventMutations[(nextCommandId - 1)];
            for (uint32_t i = 0; i < numWaitEvents; i++) {
                WaitEventVariableDescriptor &mutableWaitEvent = currentAppend.waitEvents[i];
                UNRECOVERABLE_IF(i != mutableWaitEvent.waitEventIndex);

                auto &variableSemWaitCmdList = mutableWaitEvent.eventVariable->getSemWaitList();
                auto &variableLoadRegImmCmdList = mutableWaitEvent.eventVariable->getLoadRegImmList();

                for (uint32_t packet = 0; packet < mutableWaitEvent.waitEventPackets; packet++) {
                    if (CommandListImp::isInOrderExecutionEnabled() && mutableWaitEvent.event->isCounterBased() && (this->heaplessModeEnabled || !mutableWaitEvent.event->hasInOrderTimestampNode())) {
                        captureCounterBasedWaitEventCommands(waitEventCmdToPatchIterator, variableSemWaitCmdList, variableLoadRegImmCmdList);
                    } else {
                        captureRegularWaitEventCommands(waitEventCmdToPatchIterator, variableSemWaitCmdList);
                    }
                }
            }
        }
        this->appendCmdsToPatch.clear();
        this->nextAppendKernelMutable = false;
        this->nextMutationFlags = 0;
        this->appendKernelMutableComputeWalker = nullptr;
    }
    return retCode;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(Kernel *kernel, const ze_group_count_t &threadGroupDimensions, Event *event, CmdListKernelLaunchParams &launchParams) {

    if (launchParams.isIndirect || launchParams.isBuiltInKernel) {
        if (this->nextAppendKernelMutable) {
            // active get next command id
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        } else {
            return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);
        }
    }

    const auto &kernelAttributes = kernel->getKernelDescriptor().kernelAttributes;
    const auto &payloadMappings = kernel->getKernelDescriptor().payloadMappings;

    const bool usesUnsupportedFeature = kernelAttributes.flags.requiresImplicitArgs;
    if (usesUnsupportedFeature) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE;
    }

    MutableAppendLaunchKernelWithParams mutableCmdlistAppendLaunchParams = {};
    KernelVariableDescriptor *currentKernelVariables = nullptr;

    if (this->nextAppendKernelMutable) {
        AppendKernelMutation &currentAppend = this->kernelMutations[(nextCommandId - 1)];
        mutableCmdlistAppendLaunchParams.mutationFlags = this->nextMutationFlags;
        if (currentAppend.kernelGroup != nullptr) {
            currentAppend.kernelGroup->setCurrentMutableKernel(kernel);

            mutableCmdlistAppendLaunchParams.currentMutableKernel = currentAppend.kernelGroup->getCurrentMutableKernel();
            mutableCmdlistAppendLaunchParams.maxKernelGroupScratch[0] = currentAppend.kernelGroup->getMaxAppendScratchSize(0);
            mutableCmdlistAppendLaunchParams.maxKernelGroupScratch[1] = currentAppend.kernelGroup->getMaxAppendScratchSize(1);
            mutableCmdlistAppendLaunchParams.maxKernelGroupIndirectHeap = currentAppend.kernelGroup->getMaxAppendIndirectHeapSize();
            mutableCmdlistAppendLaunchParams.extraPayloadSpaceForKernelGroup = (mutableCmdlistAppendLaunchParams.maxKernelGroupIndirectHeap - mutableCmdlistAppendLaunchParams.currentMutableKernel->getKernel()->getIndirectSize());
            mutableCmdlistAppendLaunchParams.kernelMutation = true;
            mutableCmdlistAppendLaunchParams.requiredDispatchWalkOrderFromApi = launchParams.requiredDispatchWalkOrder;
            mutableCmdlistAppendLaunchParams.requiredPartitionDimFromApi = launchParams.requiredPartitionDim;
            mutableCmdlistAppendLaunchParams.localRegionSizeFromApi = launchParams.localRegionSize;
            mutableCmdlistAppendLaunchParams.isCooperativeFromApi = launchParams.isCooperative;

            currentKernelVariables = &mutableCmdlistAppendLaunchParams.currentMutableKernel->getKernelVariables();
        } else {
            currentKernelVariables = &currentAppend.variables;
        }

        storeKernelArgumentAndDispatchVariables(mutableCmdlistAppendLaunchParams, launchParams, kernel, currentKernelVariables);

        if (mutableCmdlistAppendLaunchParams.kernelMutation) {
            launchParams.reserveExtraPayloadSpace += mutableCmdlistAppendLaunchParams.extraPayloadSpaceForKernelGroup;
            launchParams.externalPerThreadScratchSize[0] = mutableCmdlistAppendLaunchParams.maxKernelGroupScratch[0];
            launchParams.externalPerThreadScratchSize[1] = mutableCmdlistAppendLaunchParams.maxKernelGroupScratch[1];
            launchParams.omitAddingKernelInternalResidency = true;
        }

        if (mutableCmdlistAppendLaunchParams.kernelArgumentMutation) {
            launchParams.omitAddingKernelArgumentResidency = true;
            auto &residencyContainer = kernel->getArgumentsResidencyContainer();
            for (auto resource : residencyContainer) {
                addToResidencyContainer(resource);
            }
        }
    }
    launchParams.cmdWalkerBuffer = MutableComputeWalkerHw<GfxFamily>::createCommandBuffer();
    auto retVal = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }
    auto walkerPtr = std::make_unique<MutableComputeWalkerHw<GfxFamily>>(launchParams.outWalker,
                                                                         payloadMappings.implicitArgs.indirectDataPointerAddress.offset,
                                                                         payloadMappings.implicitArgs.scratchPointerAddress.offset,
                                                                         launchParams.cmdWalkerBuffer,
                                                                         this->hasStageCommitVariables);
    mutableWalkerCmds.emplace_back(std::move(walkerPtr));
    this->appendKernelMutableComputeWalker = (*mutableWalkerCmds.rbegin()).get();
    retVal = this->parseDispatchedKernel(kernel, appendKernelMutableComputeWalker, mutableCmdlistAppendLaunchParams.extraPayloadSpaceForKernelGroup,
                                         static_cast<L0::KernelImp *>(kernel)->getSyncBufferAllocation(),
                                         static_cast<L0::KernelImp *>(kernel)->getRegionGroupBarrierAllocation());
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    if (this->nextAppendKernelMutable) {
        auto appendKernelDispatch = (*dispatches.rbegin()).get();
        if (appendKernelDispatch->syncBuffer != nullptr) {
            size_t threadGroupCount = threadGroupDimensions.groupCountX * threadGroupDimensions.groupCountY * threadGroupDimensions.groupCountZ;
            appendKernelDispatch->syncBufferSize = NEO::KernelHelper::getSyncBufferSize(threadGroupCount);
            appendKernelDispatch->syncBufferNoopPatchIndex = launchParams.syncBufferPatchIndex;
        }
        if (appendKernelDispatch->regionBarrier != nullptr) {
            size_t threadGroupCount = threadGroupDimensions.groupCountX * threadGroupDimensions.groupCountY * threadGroupDimensions.groupCountZ;
            appendKernelDispatch->regionBarrierSize = NEO::KernelHelper::getRegionGroupBarrierSize(threadGroupCount, launchParams.localRegionSize);
            appendKernelDispatch->regionBarrierNoopPatchIndex = launchParams.regionBarrierPatchIndex;
        }

        if (mutableCmdlistAppendLaunchParams.kernelMutation) {
            // base kernel from append has current allocations, these must be imported into mutable residency, their position saved for future updates of allocations
            mutableCmdlistAppendLaunchParams.currentMutableKernel->saveResidencyAllocationIndices(static_cast<L0::KernelImp *>(kernel)->getSyncBufferIndex(),
                                                                                                  static_cast<L0::KernelImp *>(kernel)->getRegionGroupBarrierIndex());
            mutableCmdlistAppendLaunchParams.currentMutableKernel->makeKernelResidencySnapshotContainer(true);

            auto &residencyContainer = mutableCmdlistAppendLaunchParams.currentMutableKernel->getKernelResidencySnapshotContainer();
            for (auto resource : residencyContainer) {
                addToResidencyContainer(resource);
            }

            mutableCmdlistAppendLaunchParams.currentMutableKernel->setComputeWalker(this->appendKernelMutableComputeWalker);
            mutableCmdlistAppendLaunchParams.currentMutableKernel->setKernelDispatch(appendKernelDispatch);
            mutableCmdlistAppendLaunchParams.currentMutableKernel->createHostViewIndirectData(false);

            AppendKernelMutation &currentAppend = this->kernelMutations[(nextCommandId - 1)];

            if (launchParams.outListCommands) {
                auto prefetchToPatch = std::find_if(launchParams.outListCommands->begin(), launchParams.outListCommands->end(),
                                                    [](const CommandToPatch &cmd) { return cmd.type == CommandToPatch::CommandType::PrefetchKernelMemory; });

                if (prefetchToPatch != launchParams.outListCommands->end()) {
                    currentAppend.kernelGroup->setPrefetchCmd(*prefetchToPatch);

                    auto ioh = this->commandContainer.getIndirectHeap(NEO::IndirectHeapType::indirectObject);
                    currentAppend.kernelGroup->setIohForPrefetch(ioh->getGraphicsAllocation());
                }
            }

            if (currentAppend.kernelGroup->isScratchNeeded()) {
                currentAppend.kernelGroup->setScratchAddressPatchIndex(launchParams.scratchAddressPatchIndex);
            }

            for (auto &mutableViewKernel : currentAppend.kernelGroup->getKernelsInGroup()) {
                if (mutableViewKernel->getKernel() == kernel) {
                    continue;
                }
                retVal = captureKernelGroupVariablesAndCommandView(mutableViewKernel.get(),
                                                                   launchParams.outWalker,
                                                                   threadGroupDimensions,
                                                                   event,
                                                                   mutableCmdlistAppendLaunchParams);
                if (retVal != ZE_RESULT_SUCCESS) {
                    return retVal;
                }
            }
        }

        if (mutableCmdlistAppendLaunchParams.kernelArgumentMutation) {
            auto &appendKernelExt = MclKernelExt::get(kernel);
            // variables are already initialized with set kernel arg memory/immediate values - reflect it in variable state
            auto &residencyContainer = kernel->getArgumentsResidencyContainer();
            const auto &kernelArgInfos = static_cast<L0::KernelImp *>(kernel)->getKernelArgInfos();
            auto &kernelVariables = appendKernelExt.getVariables();
            for (uint32_t index = 0; index < kernelVariables.size(); index++) {
                auto variable = kernelVariables[index];
                if (variable == nullptr) {
                    continue;
                }
                auto &varDescriptor = variable->getDesc();
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument variable %p index %u type %" PRIu8 "\n", variable, index, varDescriptor.type);
                if (varDescriptor.type == VariableType::buffer) {
                    if (residencyContainer[index] != nullptr) {
                        varDescriptor.bufferAlloc = residencyContainer[index];
                        varDescriptor.argValue = kernelArgInfos[index].value;
                        varDescriptor.bufferGpuAddress = varDescriptor.bufferAlloc->getGpuAddress();
                        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument buffer gpuva %" PRIx64 " ptr value %p\n", varDescriptor.bufferGpuAddress, kernelArgInfos[index].value);
                        varDescriptor.state = VariableDescriptor::State::initialized;
                    } else {
                        varDescriptor.argValue = nullptr;
                        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument nullptr buffer\n");
                    }
                }
            }
            // clean variable list in case next append kernel is immutable
            appendKernelExt.cleanArgumentVariables();
        }
        if (mutableCmdlistAppendLaunchParams.groupCountVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.groupSizeVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.globalOffsetVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.lastSlmArgumentVariable != nullptr) {

            uint32_t initialGroupCount[3] = {threadGroupDimensions.groupCountX, threadGroupDimensions.groupCountY, threadGroupDimensions.groupCountZ};
            MutableKernelDispatchParameters dispatchParams = {
                initialGroupCount,                                             // groupCount
                static_cast<L0::KernelImp *>(kernel)->getGroupSize(),          // groupSize
                static_cast<L0::KernelImp *>(kernel)->getGlobalOffsets(),      // globalOffset
                kernel->getPerThreadDataSizeForWholeThreadGroup(),             // perThreadSize
                kernel->getRequiredWorkgroupOrder(),                           // walkOrder
                kernel->getNumThreadsPerThreadGroup(),                         // numThreadsPerThreadGroup
                kernel->getThreadExecutionMask(),                              // threadExecutionMask
                kernel->getMaxWgCountPerTile(getBase()->getEngineGroupType()), // maxWorkGroupCountPerTile
                0,                                                             // maxCooperativeGroupCount
                launchParams.localRegionSize,                                  // localRegionSize
                launchParams.requiredPartitionDim,                             // requiredPartitionDim
                launchParams.requiredDispatchWalkOrder,                        // requiredDispatchWalkOrder
                kernel->requiresGenerationOfLocalIdsByRuntime(),               // generationOfLocalIdsByRuntime
                launchParams.isCooperative};                                   // cooperativeDispatch

            if (launchParams.isCooperative) {
                dispatchParams.maxCooperativeGroupCount = kernel->suggestMaxCooperativeGroupCount(base->getEngineGroupType(), false);
            }

            retVal = addVariableDispatch(kernel->getKernelDescriptor(), *appendKernelDispatch,
                                         mutableCmdlistAppendLaunchParams.groupSizeVariable,
                                         mutableCmdlistAppendLaunchParams.groupCountVariable,
                                         mutableCmdlistAppendLaunchParams.globalOffsetVariable,
                                         mutableCmdlistAppendLaunchParams.lastSlmArgumentVariable,
                                         this->appendKernelMutableComputeWalker, dispatchParams);

            if (retVal != ZE_RESULT_SUCCESS) {
                return retVal;
            }
            this->enableReservePerThreadForLocalId = false;
        }
    }
    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::close() {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto baseClosed = this->baseCmdListClosed;
    if (this->baseCmdListClosed == false) {
        result = CommandListCoreFamily<gfxCoreFamily>::close();
        this->baseCmdListClosed = true;
    } else {
        if (this->updatedCommandList == true) {
            if (this->hasStageCommitVariables) {
                // traverse through all staged variables and commit them
                for (auto &variable : this->stageCommitVariables) {
                    if (variable->getDesc().commitRequired) {
                        variable->commitVariable();
                    }
                }
            }
        }
    }
    // baseClosed==false means 1st closure after definition
    // updatedCommandList==true means any update/mutation was done and close processing is needed
    if (baseClosed == false || this->updatedCommandList == true) {
        this->stageCommitVariables.clear();
        this->cooperativeKernelVariableDispatches.clear();
        processResidencyContainer(baseClosed);
        this->updatedCommandList = false;
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::reset() {
    ze_result_t result = CommandListCoreFamily<gfxCoreFamily>::reset();

    this->allocs.sshAlloc.reset(nullptr);
    this->allocs.constsAlloc.reset(nullptr);
    this->allocs.ihAlloc.reset(nullptr);
    this->allocs.varsAlloc.reset(nullptr);
    this->allocs.stringsAlloc.reset(nullptr);

    this->nativeBinary.clear();
    this->labelStorage.clear();
    this->labelMap.clear();
    this->variableStorage.clear();
    this->variableMap.clear();
    this->kernelData.clear();
    this->dispatches.clear();
    this->sbaVec.clear();
    this->kernelMutations.clear();
    this->eventMutations.clear();
    this->mutableWalkerCmds.clear();
    this->mutablePipeControlCmds.clear();
    this->mutableStoreRegMemCmds.clear();
    this->mutableSemaphoreWaitCmds.clear();
    this->mutableStoreDataImmCmds.clear();
    this->mutableLoadRegisterImmCmds.clear();
    this->mutableKernelGroups.clear();
    this->cooperativeKernelVariableDispatches.clear();
    this->appendCmdsToPatch.clear();
    this->stageCommitVariables.clear();

    this->tempMem.variables.clear();
    this->tempMem.eleCount = 0;

    this->mutableAllocations.cleanResidencyContainer();

    this->nextCommandId = 0;
    this->nextMutationFlags = 0;
    this->appendKernelMutableComputeWalker = nullptr;

    this->nextAppendKernelMutable = false;
    this->baseCmdListClosed = false;
    this->hasStageCommitVariables = false;
    this->updatedCommandList = false;
    this->finalizeCommandListResidency = true;

    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::switchCounterBasedEvents(uint64_t inOrderExecBaseSignalValue, uint32_t inOrderAllocationOffset, Event *newEvent) {
    newEvent->updateInOrderExecState(CommandListImp::inOrderExecInfo, inOrderExecBaseSignalValue, inOrderAllocationOffset);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureCounterBasedWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                                                                       std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                                                       std::vector<MutableLoadRegisterImm *> &variableLoadRegisterImmList) {

    bool qwordIndirect = isQwordInOrderCounter();
    if (qwordIndirect) {
        auto &loadRegImmCmdToPatch = *cmdsIterator;
        UNRECOVERABLE_IF(loadRegImmCmdToPatch.type != CommandToPatch::CbWaitEventLoadRegisterImm);

        auto loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch.pDestination,
                                                                                   static_cast<uint32_t>(loadRegImmCmdToPatch.offset),
                                                                                   loadRegImmCmdToPatch.inOrderPatchListIndex);
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        auto loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;

        loadRegImmCmdToPatch = *cmdsIterator;
        UNRECOVERABLE_IF(loadRegImmCmdToPatch.type != CommandToPatch::CbWaitEventLoadRegisterImm);

        loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch.pDestination,
                                                                              static_cast<uint32_t>(loadRegImmCmdToPatch.offset),
                                                                              loadRegImmCmdToPatch.inOrderPatchListIndex);
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;
    }
    auto &semaphoreWaitCmdToPatch = *cmdsIterator;
    UNRECOVERABLE_IF(semaphoreWaitCmdToPatch.type != CommandToPatch::CbWaitEventSemaphoreWait);

    auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitCmdToPatch.pDestination,
                                                                          semaphoreWaitCmdToPatch.offset,
                                                                          semaphoreWaitCmdToPatch.inOrderPatchListIndex,
                                                                          MutableSemaphoreWait::cbEventWait,
                                                                          qwordIndirect);
    mutableSemaphoreWaitCmds.emplace_back(std::move(semWaitPtr));
    auto semWaitCmd = (*mutableSemaphoreWaitCmds.rbegin()).get();
    variableSemaphoreWaitList.emplace_back(semWaitCmd);

    ++cmdsIterator;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureRegularWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                                                                  std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList) {
    auto &semaphoreWaitCmdToPatch = *cmdsIterator;
    UNRECOVERABLE_IF(semaphoreWaitCmdToPatch.type != CommandToPatch::WaitEventSemaphoreWait);

    auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitCmdToPatch.pDestination,
                                                                          semaphoreWaitCmdToPatch.offset,
                                                                          0,
                                                                          MutableSemaphoreWait::regularEventWait,
                                                                          false);
    mutableSemaphoreWaitCmds.emplace_back(std::move(semWaitPtr));
    auto semWaitCmd = (*mutableSemaphoreWaitCmds.rbegin()).get();
    variableSemaphoreWaitList.emplace_back(semWaitCmd);

    ++cmdsIterator;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureCounterBasedTimestampSignalEventCommands(SignalEventVariableDescriptor &currentMutableSignalEvent,
                                                                                                  std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                                                                  std::vector<MutableStoreDataImm *> &variableStoreDataImmList) {
    auto partitionCount = CommandListCoreFamily<gfxCoreFamily>::getPartitionCount();
    uint32_t syncWaitEventPackets = currentMutableSignalEvent.event->getPacketsToWait();
    uint32_t clearEventOps = syncWaitEventPackets / partitionCount;

    variableSemaphoreWaitList.reserve(syncWaitEventPackets);
    variableStoreDataImmList.reserve(clearEventOps);

    auto cmdsBeginIterator = this->appendCmdsToPatch.begin();
    for (; cmdsBeginIterator != this->appendCmdsToPatch.end(); ++cmdsBeginIterator) {
        auto &cbTimestampEventCmdToPatch = *cmdsBeginIterator;
        if (cbTimestampEventCmdToPatch.type == CommandToPatch::CbEventTimestampClearStoreDataImm) {
            auto storeDataImmPtr = std::make_unique<MutableStoreDataImmHw<GfxFamily>>(cbTimestampEventCmdToPatch.pDestination,
                                                                                      cbTimestampEventCmdToPatch.offset,
                                                                                      partitionCount > 1);
            mutableStoreDataImmCmds.emplace_back(std::move(storeDataImmPtr));
            auto storeDataImm = (*mutableStoreDataImmCmds.rbegin()).get();
            variableStoreDataImmList.emplace_back(storeDataImm);
        } else if (cbTimestampEventCmdToPatch.type == CommandToPatch::CbEventTimestampPostSyncSemaphoreWait) {
            auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(cbTimestampEventCmdToPatch.pDestination,
                                                                                  cbTimestampEventCmdToPatch.offset,
                                                                                  0,
                                                                                  MutableSemaphoreWait::cbEventTimestampSyncWait,
                                                                                  false);
            mutableSemaphoreWaitCmds.emplace_back(std::move(semWaitPtr));
            auto semWaitCmd = (*mutableSemaphoreWaitCmds.rbegin()).get();
            variableSemaphoreWaitList.emplace_back(semWaitCmd);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureStandaloneTimestampSignalEventCommands(std::vector<MutableStoreRegisterMem *> &variableStoreRegisterMem) {
    auto cmdsReverseBeginIterator = this->appendCmdsToPatch.rbegin();
    for (; cmdsReverseBeginIterator != this->appendCmdsToPatch.rend(); ++cmdsReverseBeginIterator) {
        auto &cmdToPatch = *cmdsReverseBeginIterator;
        if (cmdToPatch.type != CommandToPatch::TimestampEventPostSyncStoreRegMem) {
            continue;
        }
        auto storeRegMemPtr = std::make_unique<MutableStoreRegisterMemHw<GfxFamily>>(cmdToPatch.pDestination, cmdToPatch.offset);
        mutableStoreRegMemCmds.emplace_back(std::move(storeRegMemPtr));
        auto storeRegMem = (*mutableStoreRegMemCmds.rbegin()).get();
        variableStoreRegisterMem.emplace_back(storeRegMem);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::storeKernelArgumentAndDispatchVariables(MutableAppendLaunchKernelWithParams &mutableParams,
                                                                                          CmdListKernelLaunchParams &launchParams,
                                                                                          Kernel *kernel,
                                                                                          KernelVariableDescriptor *kernelVariables) {
    if ((mutableParams.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT) == ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT) {
        InterfaceVariableDescriptor varDesc = {};
        varDesc.isStageCommit = true;
        getVariable(&varDesc, &mutableParams.groupCountVariable);

        kernelVariables->groupCount = mutableParams.groupCountVariable;
    }
    if ((mutableParams.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE) == ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE) {
        InterfaceVariableDescriptor varDesc = {};
        varDesc.isStageCommit = true;
        getVariable(&varDesc, &mutableParams.groupSizeVariable);

        kernelVariables->groupSize = mutableParams.groupSizeVariable;

        this->enableReservePerThreadForLocalId = true;
        launchParams.reserveExtraPayloadSpace = this->maxPerThreadDataSize;
    }
    if ((mutableParams.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET) == ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET) {
        InterfaceVariableDescriptor varDesc = {};
        getVariable(&varDesc, &mutableParams.globalOffsetVariable);

        kernelVariables->globalOffset = mutableParams.globalOffsetVariable;
    }
    if ((mutableParams.mutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS) == ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS) {
        // intercept kernel arguments
        auto &kernelArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
        kernelVariables->kernelArguments.reserve(kernelArgs.size());
        uint32_t argCount = 0;
        for (const auto &arg : kernelArgs) {
            KernelArgumentVariableDescriptor mutableKernelArgumentDesc = {};
            mutableKernelArgumentDesc.argIndex = argCount;
            bool captureArgument = false;
            bool slmArgument = false;
            bool immediateArgument = arg.type == NEO::ArgDescriptor::argTValue;
            if (arg.type == NEO::ArgDescriptor::argTPointer) {
                captureArgument = arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrGlobal ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrConstant ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrUnknown ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal;

                slmArgument = arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal;
            }
            captureArgument |= immediateArgument;
            if (captureArgument) {
                Variable *variable = nullptr;
                InterfaceVariableDescriptor varDesc = {};
                varDesc.isStageCommit = slmArgument;
                varDesc.immediateValueChunks = immediateArgument;
                getVariable(&varDesc, &variable);
                variable->setAsKernelArg(kernel->toHandle(), argCount);

                mutableKernelArgumentDesc.kernelArgumentVariable = variable;

                kernelVariables->kernelArguments.push_back(mutableKernelArgumentDesc);

                mutableParams.kernelArgumentMutation = true;

                if (slmArgument) {
                    if (mutableParams.lastSlmArgumentVariable != nullptr) {
                        mutableParams.lastSlmArgumentVariable->setNextSlmVariable(variable);
                    }
                    mutableParams.lastSlmArgumentVariable = variable;
                }
            } else {
                // in case kernel argument is non-mutable (not captured), push it into kernel arguments vector anyway to preserve index order
                kernelVariables->kernelArguments.push_back(mutableKernelArgumentDesc);
            }

            argCount++;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::storeSignalEventVariable(MutableAppendLaunchKernelEvents &mutableEventParams,
                                                                           CmdListKernelLaunchParams &launchParams,
                                                                           Event *event) {
    if ((this->nextMutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT) == ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT) {
        if (event != nullptr) {
            AppendEventMutation &currentAppend = this->eventMutations[(nextCommandId - 1)];

            Variable *variable = nullptr;
            InterfaceVariableDescriptor varDesc = {};
            getVariable(&varDesc, &variable);
            mutableEventParams.signalEvent = true;

            launchParams.omitAddingEventResidency = event->getAllocation(this->device) != nullptr;

            mutableEventParams.l3FlushEvent = CommandListCoreFamily<gfxCoreFamily>::compactL3FlushEvent(CommandListCoreFamily<gfxCoreFamily>::getDcFlushRequired(event->isSignalScope()));
            if (CommandListImp::isInOrderExecutionEnabled()) {
                mutableEventParams.eventInsideInOrder = true;
                mutableEventParams.counterBasedEvent = event->isCounterBased();
                mutableEventParams.inOrderIncrementEvent = event->getInOrderIncrementValue(this->partitionCount) > 0;
                if (mutableEventParams.counterBasedEvent && CommandListCoreFamily<gfxCoreFamily>::duplicatedInOrderCounterStorageEnabled) {
                    mutableEventParams.counterBasedTimestampEvent = event->isEventTimestampFlagSet();
                }
                if (CommandListCoreFamily<gfxCoreFamily>::isInOrderNonWalkerSignalingRequired(event)) {
                    // this command is internally sync by clean StoreDataImm and sync SemWait
                    if (launchParams.outListCommands == nullptr) {
                        launchParams.outListCommands = &this->appendCmdsToPatch;
                    }
                    if (event->isEventTimestampFlagSet()) {
                        if (mutableEventParams.counterBasedEvent) {
                            mutableEventParams.counterBasedTimestampEvent = true;
                        }
                        if (mutableEventParams.l3FlushEvent) {
                            mutableEventParams.l3FlushEventTimestampSyncCmds = true;
                        }
                    } else if (mutableEventParams.l3FlushEvent) {
                        launchParams.outSyncCommand = &mutableEventParams.signalCmd;
                        mutableEventParams.l3FlushEventSyncCmd = true;
                    }
                }
                launchParams.omitAddingEventResidency |= (mutableEventParams.l3FlushEvent || mutableEventParams.counterBasedTimestampEvent);
            } else {
                if (mutableEventParams.l3FlushEvent) {
                    if (event->isEventTimestampFlagSet()) {
                        mutableEventParams.l3FlushEventTimestampSyncCmds = true;
                    } else {
                        mutableEventParams.l3FlushEventSyncCmd = true;
                    }
                }
                if (mutableEventParams.l3FlushEventSyncCmd) {
                    launchParams.outSyncCommand = &mutableEventParams.signalCmd;
                } else if (mutableEventParams.l3FlushEventTimestampSyncCmds) {
                    if (launchParams.outListCommands == nullptr) {
                        launchParams.outListCommands = &this->appendCmdsToPatch;
                    }
                }
            }

            currentAppend.signalEvent.event = event;
            currentAppend.signalEvent.eventVariable = variable;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::captureKernelGroupVariablesAndCommandView(MutableKernel *mutableKernel,
                                                                                                   void *batchBufferWalker,
                                                                                                   const ze_group_count_t &threadGroupDimensions,
                                                                                                   Event *event,
                                                                                                   MutableAppendLaunchKernelWithParams &parentMutableAppendLaunchParams) {
    MutableAppendLaunchKernelWithParams viewKernelAppendLaunchParams = {};
    CmdListKernelLaunchParams launchParams = {};
    auto viewKernel = mutableKernel->getKernel();
    auto &payloadMappings = viewKernel->getKernelDescriptor().payloadMappings;
    auto viewKernelVariableDescriptors = &mutableKernel->getKernelVariables();
    viewKernelAppendLaunchParams.mutationFlags = parentMutableAppendLaunchParams.mutationFlags;

    storeKernelArgumentAndDispatchVariables(viewKernelAppendLaunchParams, launchParams, viewKernel, viewKernelVariableDescriptors);
    mutableKernel->allocateHostViewIndirectHeap();

    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = MutableComputeWalkerHw<GfxFamily>::createCommandBuffer();
    launchParams.hostPayloadBuffer = mutableKernel->getHostViewIndirectHeap();
    launchParams.omitAddingKernelArgumentResidency = true;
    launchParams.omitAddingKernelInternalResidency = true;
    launchParams.omitAddingEventResidency = true;
    launchParams.omitAddingWaitEventsResidency = true;
    launchParams.requiredDispatchWalkOrder = parentMutableAppendLaunchParams.requiredDispatchWalkOrderFromApi;
    launchParams.requiredPartitionDim = parentMutableAppendLaunchParams.requiredPartitionDimFromApi;
    launchParams.localRegionSize = parentMutableAppendLaunchParams.localRegionSizeFromApi;
    launchParams.isCooperative = parentMutableAppendLaunchParams.isCooperativeFromApi;

    auto retVal = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(viewKernel, threadGroupDimensions, event, launchParams);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    auto walkerPtr = std::make_unique<MutableComputeWalkerHw<GfxFamily>>(batchBufferWalker,
                                                                         payloadMappings.implicitArgs.indirectDataPointerAddress.offset,
                                                                         payloadMappings.implicitArgs.scratchPointerAddress.offset,
                                                                         launchParams.cmdWalkerBuffer,
                                                                         this->hasStageCommitVariables);
    mutableWalkerCmds.emplace_back(std::move(walkerPtr));
    auto viewKernelMutableComputeWalker = (*mutableWalkerCmds.rbegin()).get();
    retVal = this->parseDispatchedKernel(viewKernel,
                                         viewKernelMutableComputeWalker,
                                         (parentMutableAppendLaunchParams.maxKernelGroupIndirectHeap - mutableKernel->getKernel()->getIndirectSize()),
                                         nullptr,
                                         nullptr);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }
    auto viewKernelDispatch = (*dispatches.rbegin()).get();

    mutableKernel->setComputeWalker(viewKernelMutableComputeWalker);
    mutableKernel->setKernelDispatch(viewKernelDispatch);
    mutableKernel->createHostViewIndirectData(true);

    auto &viewKernelExt = MclKernelExt::get(viewKernel);
    viewKernelExt.cleanArgumentVariables();

    uint32_t initialGroupCount[3] = {threadGroupDimensions.groupCountX, threadGroupDimensions.groupCountY, threadGroupDimensions.groupCountZ};

    MutableKernelDispatchParameters dispatchParams = {
        initialGroupCount,                                                 // groupCount
        static_cast<L0::KernelImp *>(viewKernel)->getGroupSize(),          // groupSize
        static_cast<L0::KernelImp *>(viewKernel)->getGlobalOffsets(),      // globalOffset
        viewKernel->getPerThreadDataSizeForWholeThreadGroup(),             // perThreadSize
        viewKernel->getRequiredWorkgroupOrder(),                           // walkOrder
        viewKernel->getNumThreadsPerThreadGroup(),                         // numThreadsPerThreadGroup
        viewKernel->getThreadExecutionMask(),                              // threadExecutionMask
        viewKernel->getMaxWgCountPerTile(getBase()->getEngineGroupType()), // maxWorkGroupCountPerTile
        0,                                                                 // maxCooperativeGroupCount
        launchParams.localRegionSize,                                      // localRegionSize
        launchParams.requiredPartitionDim,                                 // requiredPartitionDim
        launchParams.requiredDispatchWalkOrder,                            // requiredDispatchWalkOrder
        viewKernel->requiresGenerationOfLocalIdsByRuntime(),               // generationOfLocalIdsByRuntime
        launchParams.isCooperative};                                       // cooperativeDispatch

    retVal = addVariableDispatch(viewKernel->getKernelDescriptor(), *viewKernelDispatch,
                                 viewKernelAppendLaunchParams.groupSizeVariable,
                                 viewKernelAppendLaunchParams.groupCountVariable,
                                 viewKernelAppendLaunchParams.globalOffsetVariable,
                                 viewKernelAppendLaunchParams.lastSlmArgumentVariable,
                                 viewKernelMutableComputeWalker, dispatchParams);

    viewKernelAppendLaunchParams.groupCountVariable->resetGroupCountVariable();
    if (viewKernelAppendLaunchParams.groupSizeVariable != nullptr) {
        viewKernelAppendLaunchParams.groupSizeVariable->resetGroupSizeVariable();
    }
    if (viewKernelAppendLaunchParams.globalOffsetVariable != nullptr) {
        viewKernelAppendLaunchParams.globalOffsetVariable->resetGlobalOffsetVariable();
    }
    if (viewKernelAppendLaunchParams.lastSlmArgumentVariable != nullptr) {
        viewKernelAppendLaunchParams.lastSlmArgumentVariable->resetSlmVariable();
    }

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    auto &totalNoopSpace = CommandListCoreFamily<gfxCoreFamily>::totalNoopSpace;
    UNRECOVERABLE_IF(noopPatchIndex >= commandsToPatch.size());
    auto &noopPatch = commandsToPatch[noopPatchIndex];

    if (noopPatch.pDestination == nullptr) {
        totalNoopSpace += newPatchSize;
    } else {
        if (newPatchSize > noopPatch.patchSize) {
            totalNoopSpace += (newPatchSize - noopPatch.patchSize);
        } else {
            totalNoopSpace -= (noopPatch.patchSize - newPatchSize);
        }
    }

    noopPatch.pDestination = newCpuPtr;
    noopPatch.patchSize = newPatchSize;
    noopPatch.offset = newOffset;
    noopPatch.gpuAddress = newGpuAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t MutableCommandListCoreFamily<gfxCoreFamily>::createNewCmdListNoopPatchData(void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    auto &totalNoopSpace = CommandListCoreFamily<gfxCoreFamily>::totalNoopSpace;

    size_t noopPatchIndex = commandsToPatch.size();

    totalNoopSpace += newPatchSize;

    CommandToPatch noopPatch;
    noopPatch.type = CommandToPatch::NoopSpace;
    noopPatch.offset = newOffset;
    noopPatch.pDestination = newCpuPtr;
    noopPatch.patchSize = newPatchSize;
    noopPatch.gpuAddress = newGpuAddress;

    commandsToPatch.push_back(noopPatch);

    return noopPatchIndex;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset, uint64_t &gpuAddress) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    UNRECOVERABLE_IF(noopPatchIndex >= commandsToPatch.size());
    auto &noopPatch = commandsToPatch[noopPatchIndex];

    cpuPtr = noopPatch.pDestination;
    patchSize = noopPatch.patchSize;
    offset = noopPatch.offset;
    gpuAddress = noopPatch.gpuAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::disableAddressNoopPatch(size_t noopPatchIndex) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    auto &totalNoopSpace = CommandListCoreFamily<gfxCoreFamily>::totalNoopSpace;

    UNRECOVERABLE_IF(noopPatchIndex >= commandsToPatch.size());
    auto &noopPatch = commandsToPatch[noopPatchIndex];

    noopPatch.pDestination = nullptr;

    UNRECOVERABLE_IF(totalNoopSpace < noopPatch.patchSize);
    totalNoopSpace -= noopPatch.patchSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint32_t MutableCommandListCoreFamily<gfxCoreFamily>::getInlineDataSize() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return WalkerType::getInlineDataSize();
}

} // namespace MCL
} // namespace L0
