/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/cmdlist/cmdlist_host_function_parameters.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdlist/cmdlist_wait_parameters.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/mutable_cmdlist_hw_experimental.inl"

#include <cinttypes>
#include <iterator>

namespace L0 {
namespace MCL {

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateKernelMemoryPrefetch(const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, const PatchPrefetchKernelMemory &cmdToPatch, uint64_t cmdId) {
    NEO::LinearStream cmdStream(cmdToPatch.pDestination, cmdToPatch.patchSize);

    CommandListCoreFamily<gfxCoreFamily>::prefetchKernelMemory(cmdStream, kernel, iohAllocation, cmdToPatch.offset, nullptr, cmdId, cmdToPatch.patchSize);

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
size_t MutableCommandListCoreFamily<gfxCoreFamily>::ensureCmdBufferSpaceForPrefetch() {
    if (!CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled()) {
        return 0;
    }
    auto kernelGroup = getKernelGroupForPrefetch(getPrefetchCmdId());
    if (!kernelGroup) {
        return 0;
    }

    uint32_t isaPrefetchSizeLimit = CommandList::getLimitIsaPrefetchSize();
    auto groupMaxIsaSizeToPrefetch = std::min(kernelGroup->getMaxIsaSize(), isaPrefetchSizeLimit);

    auto expectedSize = NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(kernelGroup->getMaxAppendIndirectHeapSize(), this->device->getNEODevice()->getRootDeviceEnvironment()) +
                        NEO::EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(groupMaxIsaSizeToPrefetch, this->device->getNEODevice()->getRootDeviceEnvironment());
    this->commandContainer.getCommandStream()->ensureContinuousSpace(expectedSize);

    return expectedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint32_t MutableCommandListCoreFamily<gfxCoreFamily>::getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const {
    if (auto kernelGroup = getKernelGroupForPrefetch(getPrefetchCmdId())) {
        return kernelGroup->getMaxAppendIndirectHeapSize();
    }

    return kernel.getIndirectSize() + reserveExtraSpace;
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
    this->maxPerThreadDataSize = static_cast<uint32_t>(device->getDeviceInfo().maxWorkGroupSize * 3 * sizeof(uint16_t));
    this->iohAlignment = NEO::EncodeDispatchKernel<GfxFamily>::getDefaultIOHAlignment(this->commandContainer.isIndirectHeapInLocalMemory(), device->getHwInfo());
    this->inlineDataSize = getInlineDataSize();
    this->semaphore64bCmdSupported = device->getDeviceInfo().semaphore64bCmdSupport;

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
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        if (kernelInstructionMutationEnabled(this->nextMutationFlags) && CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled()) {
            this->appendCmdsToPatch.makeCommandView = CommandListCoreFamily<gfxCoreFamily>::isPatchPreambleEnabled();
            launchParams.outListCommands = &this->appendCmdsToPatch;
            mutableEventParams.mutableCmdPatchlistContainer = &this->appendCmdsToPatch;
        }

        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);
        launchParams.omitAddingWaitEventsResidency = mutableEventParams.omitWaitEventResidency;
        launchParams.outListCommands = mutableEventParams.mutableCmdPatchlistContainer;

        Event *signalEvent = Event::fromHandle(hEvent);
        storeSignalEventVariable(mutableEventParams, launchParams, signalEvent);
    }

    auto retCode = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernelHandle, threadGroupDimensions, hEvent, numWaitEvents, phWaitEvents, launchParams);
    if (retCode != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
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
                if (mutableEventParams.counterBasedTimestampEvent) {
                    auto &eventVariableSemaphoreWaitList = signalEventVariableDesc.eventVariable->getSemWaitList();
                    auto &eventVariableStoreDataImmList = signalEventVariableDesc.eventVariable->getStoreDataImmList();

                    captureCounterBasedTimestampSignalEventCommands(signalEventVariableDesc,
                                                                    eventVariableSemaphoreWaitList, eventVariableStoreDataImmList);
                    walker = this->appendKernelMutableComputeWalker;
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
                    auto signalPipeControlPtr = std::make_unique<MutablePipeControlHw<GfxFamily>>(std::get<PatchSignalEventPostSyncPipeControl>(mutableEventParams.signalCmd).pDestination);
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
            processWaitEventVariables(numWaitEvents);
        }

        clearMutableAppendData();
    }
    return retCode;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(Kernel *kernel, const ze_group_count_t &threadGroupDimensions, Event *event, CmdListKernelLaunchParams &launchParams) {

    if (launchParams.isIndirect) {
        if (this->nextAppendKernelMutable) {
            // indirect kernels and active next command id are not supported
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        } else {
            return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);
        }
    }

    constexpr ze_mutable_command_exp_flags_t notSupportedNonKernelMutationFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                                                                                  ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET |
                                                                                  ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;
    const bool unsupportedCondition = this->nextAppendKernelMutable && (this->nextMutationFlags & notSupportedNonKernelMutationFlags) != 0;
    if (launchParams.isBuiltInKernel) {
        // builtin kernels are supported with only wait event mutation flags
        if (unsupportedCondition) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    const auto &kernelAttributes = kernel->getKernelDescriptor().kernelAttributes;
    const auto &payloadMappings = kernel->getKernelDescriptor().payloadMappings;

    const bool usesUnsupportedFeature = kernelAttributes.flags.requiresImplicitArgs;
    if (usesUnsupportedFeature) {
        // unsupported kernels cannot be mutated with unsupported flags
        if (unsupportedCondition) {
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE;
        }
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
        MutableComputeWalkerHw<GfxFamily>::deleteCommandBuffer(launchParams.cmdWalkerBuffer);
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
                                         mutableCmdlistAppendLaunchParams.firstSlmArgumentVariable, false);
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

        if (mutableCmdlistAppendLaunchParams.kernelMutation) {
            // base kernel from append has current allocations, these must be imported into mutable residency, their position saved for future updates of allocations
            mutableCmdlistAppendLaunchParams.currentMutableKernel->saveResidencyAllocationIndices(static_cast<L0::KernelImp *>(kernel)->getSyncBufferIndex());
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
                                                    [](const CommandToPatchInCmdList &cmd) { return std::holds_alternative<PatchPrefetchKernelMemory>(cmd); });

                if (prefetchToPatch != launchParams.outListCommands->end()) {
                    currentAppend.kernelGroup->setPrefetchCmd(std::get<PatchPrefetchKernelMemory>(*prefetchToPatch));

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

        uint32_t systemMemoryAllocsCount = 0u;
        uint32_t importedAllocationsCount = 0u;
        bool trackL3FlushAfterPostSync = false;

        if (mutableCmdlistAppendLaunchParams.kernelArgumentMutation) {

            trackL3FlushAfterPostSync = this->l3FlushAfterPostSyncEnabled && launchParams.isHostSignalScopeEvent;
            auto &appendKernelExt = MclKernelExt::get(kernel);
            // variables are already initialized with set kernel arg memory/immediate values - reflect it in variable state
            auto &residencyContainer = kernel->getArgumentsResidencyContainer();
            const auto &kernelArgInfos = static_cast<L0::KernelImp *>(kernel)->getKernelArgInfos();
            auto &kernelVariables = appendKernelExt.getVariables();

            if (trackL3FlushAfterPostSync) {
                buffersVariables.reserve(kernelVariables.size());
            }

            for (uint32_t index = 0; index < kernelVariables.size(); index++) {
                auto variable = kernelVariables[index];
                if (variable == nullptr) {
                    continue;
                }
                auto &varDescriptor = variable->getDesc();
                PRINT_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument variable %p index %u type %" PRIu8 "\n", variable, index, varDescriptor.type);
                if (varDescriptor.type == VariableType::buffer) {

                    if (trackL3FlushAfterPostSync) {
                        buffersVariables.push_back(variable);
                    }

                    if (kernelArgInfos[index].value != nullptr) {
                        varDescriptor.bufferAlloc = residencyContainer[index];
                        varDescriptor.argValue = kernelArgInfos[index].value;
                        varDescriptor.bufferGpuAddress = reinterpret_cast<uintptr_t>(kernelArgInfos[index].value);
                        varDescriptor.allocId = kernelArgInfos[index].allocId;
                        varDescriptor.allocIdMemoryManagerCounter = kernelArgInfos[index].allocIdMemoryManagerCounter;
                        PRINT_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument buffer gpuva %" PRIx64 " ptr value %p alloc id %u, alloc id counter %u\n",
                                     varDescriptor.bufferGpuAddress,
                                     kernelArgInfos[index].value,
                                     varDescriptor.allocId,
                                     varDescriptor.allocIdMemoryManagerCounter);
                        varDescriptor.state = VariableDescriptor::State::initialized;

                        if (trackL3FlushAfterPostSync) {

                            if (CommandList::isUsingSystemMemory(varDescriptor.argValue, varDescriptor.bufferAlloc, this->sharedSystemAllocationsAllowed)) {
                                ++systemMemoryAllocsCount;
                            }
                            if (varDescriptor.bufferAlloc != nullptr && varDescriptor.bufferAlloc->getIsImported()) {
                                ++importedAllocationsCount;
                            }
                        }
                    } else {
                        varDescriptor.argValue = nullptr;
                        PRINT_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL kernel argument nullptr buffer\n");
                    }
                }
            }
            // clean variable list in case next append kernel is immutable
            appendKernelExt.cleanArgumentVariables();
        }

        if (mutableCmdlistAppendLaunchParams.groupCountVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.groupSizeVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.globalOffsetVariable != nullptr ||
            mutableCmdlistAppendLaunchParams.lastSlmArgumentVariable != nullptr ||
            buffersVariables.size() > 0) {

            uint32_t initialGroupCount[3] = {threadGroupDimensions.groupCountX, threadGroupDimensions.groupCountY, threadGroupDimensions.groupCountZ};

            MutableKernelDispatchParameters dispatchParams = {
                .groupCount = initialGroupCount,
                .groupSize = static_cast<L0::KernelImp *>(kernel)->getGroupSize(),
                .globalOffset = static_cast<L0::KernelImp *>(kernel)->getGlobalOffsets(),
                .perThreadSize = kernel->getPerThreadDataSizeForWholeThreadGroup(),
                .walkOrder = kernel->getRequiredWorkgroupOrder(),
                .numThreadsPerThreadGroup = kernel->getNumThreadsPerThreadGroup(),
                .threadExecutionMask = kernel->getThreadExecutionMask(),
                .maxCooperativeGroupCount = 0,
                .systemMemoryAllocsCount = systemMemoryAllocsCount,
                .importedAllocationsCount = importedAllocationsCount,
                .requiredPartitionDim = launchParams.requiredPartitionDim,
                .requiredDispatchWalkOrder = launchParams.requiredDispatchWalkOrder,
                .generationOfLocalIdsByRuntime = kernel->requiresGenerationOfLocalIdsByRuntime(),
                .cooperativeDispatch = launchParams.isCooperative};

            if (launchParams.isCooperative) {
                dispatchParams.maxCooperativeGroupCount = kernel->suggestMaxCooperativeGroupCount(base->getEngineGroupType(), false);
            }

            retVal = addVariableDispatch(kernel->getKernelDescriptor(), *appendKernelDispatch,
                                         mutableCmdlistAppendLaunchParams.groupSizeVariable,
                                         mutableCmdlistAppendLaunchParams.groupCountVariable,
                                         mutableCmdlistAppendLaunchParams.globalOffsetVariable,
                                         mutableCmdlistAppendLaunchParams.lastSlmArgumentVariable,
                                         &buffersVariables,
                                         this->appendKernelMutableComputeWalker, dispatchParams);

            if (retVal != ZE_RESULT_SUCCESS) {
                buffersVariables.clear();
                return retVal;
            }
            this->enableReservePerThreadForLocalId = false;
            buffersVariables.clear();
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
        this->calculateAsyncPatchlistPatchSize();
        this->calculateActiveScratchPatchElemsPatchSize();
        this->calculateTotalNoopSpacePatchSize();
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
    newEvent->updateInOrderExecState(CommandList::inOrderExecInfo, inOrderExecBaseSignalValue, inOrderAllocationOffset);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureExternalCounterBasedWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                                                                               std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                                                               std::vector<MutableLoadRegisterImm *> &variableLoadRegisterImmList) {
    bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(isQwordInOrderCounter(), this->semaphore64bCmdSupported);
    if (qwordIndirect) {
        auto *loadRegImmCmdToPatch = std::get_if<PatchExternalCbWaitEventPreambleCounterLoadRegisterImm>(&(*cmdsIterator));
        UNRECOVERABLE_IF(loadRegImmCmdToPatch == nullptr);

        auto loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch->gpuDestination,
                                                                                   loadRegImmCmdToPatch->commandView,
                                                                                   loadRegImmCmdToPatch->pDestination,
                                                                                   static_cast<uint32_t>(loadRegImmCmdToPatch->offset),
                                                                                   MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter,
                                                                                   this->isCopyOnly(false));
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        auto loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;

        auto *loadRegImmCmdToPatch2 = std::get_if<PatchExternalCbWaitEventPreambleCounterLoadRegisterImm>(&(*cmdsIterator));
        UNRECOVERABLE_IF(loadRegImmCmdToPatch2 == nullptr);

        loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch2->gpuDestination,
                                                                              loadRegImmCmdToPatch2->commandView,
                                                                              loadRegImmCmdToPatch2->pDestination,
                                                                              static_cast<uint32_t>(loadRegImmCmdToPatch2->offset),
                                                                              MutableLoadRegisterImm::cbEventWaitLoadPatchPreambleCounter,
                                                                              this->isCopyOnly(false));
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;
    }

    auto *semaphoreWaitCmdToPatch = std::get_if<PatchExternalCbWaitEventPreambleCounterSemaphoreWait>(&(*cmdsIterator));
    UNRECOVERABLE_IF(semaphoreWaitCmdToPatch == nullptr);

    auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitCmdToPatch->gpuDestination,
                                                                          semaphoreWaitCmdToPatch->commandView,
                                                                          semaphoreWaitCmdToPatch->pDestination,
                                                                          semaphoreWaitCmdToPatch->offset,
                                                                          MutableSemaphoreWait::cbEventWaitPatchPreambleCounter,
                                                                          isQwordInOrderCounter(),
                                                                          this->semaphore64bCmdSupported);
    mutableSemaphoreWaitCmds.emplace_back(std::move(semWaitPtr));
    auto semWaitCmd = (*mutableSemaphoreWaitCmds.rbegin()).get();
    variableSemaphoreWaitList.emplace_back(semWaitCmd);

    ++cmdsIterator;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureCounterBasedWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                                                                       std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList,
                                                                                       std::vector<MutableLoadRegisterImm *> &variableLoadRegisterImmList) {

    bool qwordIndirect = NEO::InOrderProgrammingHelpers::isLriFor64bDataProgrammingRequired(isQwordInOrderCounter(), this->semaphore64bCmdSupported);
    if (qwordIndirect) {
        auto *loadRegImmCmdToPatch = std::get_if<PatchCbWaitEventLoadRegisterImm>(&(*cmdsIterator));
        UNRECOVERABLE_IF(loadRegImmCmdToPatch == nullptr);

        auto loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch->gpuDestination,
                                                                                   loadRegImmCmdToPatch->commandView,
                                                                                   loadRegImmCmdToPatch->pDestination,
                                                                                   static_cast<uint32_t>(loadRegImmCmdToPatch->offset),
                                                                                   MutableLoadRegisterImm::cbEventWaitLoadCounter,
                                                                                   this->isCopyOnly(false));
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        auto loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;

        auto *loadRegImmCmdToPatch2 = std::get_if<PatchCbWaitEventLoadRegisterImm>(&(*cmdsIterator));
        UNRECOVERABLE_IF(loadRegImmCmdToPatch2 == nullptr);

        loadRegImmPtr = std::make_unique<MutableLoadRegisterImmHw<GfxFamily>>(loadRegImmCmdToPatch2->gpuDestination,
                                                                              loadRegImmCmdToPatch2->commandView,
                                                                              loadRegImmCmdToPatch2->pDestination,
                                                                              static_cast<uint32_t>(loadRegImmCmdToPatch2->offset),
                                                                              MutableLoadRegisterImm::cbEventWaitLoadCounter,
                                                                              this->isCopyOnly(false));
        mutableLoadRegisterImmCmds.emplace_back(std::move(loadRegImmPtr));
        loadRegImmCmd = (*mutableLoadRegisterImmCmds.rbegin()).get();
        variableLoadRegisterImmList.emplace_back(loadRegImmCmd);

        ++cmdsIterator;
    }

    auto *semaphoreWaitCmdToPatch = std::get_if<PatchCbWaitEventSemaphoreWait>(&(*cmdsIterator));
    UNRECOVERABLE_IF(semaphoreWaitCmdToPatch == nullptr);

    auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitCmdToPatch->gpuDestination,
                                                                          semaphoreWaitCmdToPatch->commandView,
                                                                          semaphoreWaitCmdToPatch->pDestination,
                                                                          semaphoreWaitCmdToPatch->offset,
                                                                          MutableSemaphoreWait::cbEventWait,
                                                                          isQwordInOrderCounter(),
                                                                          this->semaphore64bCmdSupported);
    mutableSemaphoreWaitCmds.emplace_back(std::move(semWaitPtr));
    auto semWaitCmd = (*mutableSemaphoreWaitCmds.rbegin()).get();
    variableSemaphoreWaitList.emplace_back(semWaitCmd);

    ++cmdsIterator;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::captureRegularWaitEventCommands(CommandToPatchContainer::iterator &cmdsIterator,
                                                                                  std::vector<MutableSemaphoreWait *> &variableSemaphoreWaitList) {

    auto *semaphoreWaitCmdToPatch = std::get_if<PatchWaitEventSemaphoreWait>(&(*cmdsIterator));
    UNRECOVERABLE_IF(semaphoreWaitCmdToPatch == nullptr);

    auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitCmdToPatch->gpuDestination,
                                                                          semaphoreWaitCmdToPatch->commandView,
                                                                          semaphoreWaitCmdToPatch->pDestination,
                                                                          semaphoreWaitCmdToPatch->offset,
                                                                          MutableSemaphoreWait::regularEventWait,
                                                                          false,
                                                                          this->semaphore64bCmdSupported);
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
        auto &cmdToPatch = *cmdsBeginIterator;
        if (auto *storePatch = std::get_if<PatchCbEventTimestampClearStoreDataImm>(&cmdToPatch)) {
            auto storeDataImmPtr = std::make_unique<MutableStoreDataImmHw<GfxFamily>>(storePatch->pDestination,
                                                                                      storePatch->offset,
                                                                                      partitionCount > 1);
            mutableStoreDataImmCmds.emplace_back(std::move(storeDataImmPtr));
            auto storeDataImm = (*mutableStoreDataImmCmds.rbegin()).get();
            variableStoreDataImmList.emplace_back(storeDataImm);
        } else if (auto *semaphoreWaitPatch = std::get_if<PatchCbEventTimestampPostSyncSemaphoreWait>(&cmdToPatch)) {
            auto semWaitPtr = std::make_unique<MutableSemaphoreWaitHw<GfxFamily>>(semaphoreWaitPatch->gpuDestination,
                                                                                  semaphoreWaitPatch->commandView,
                                                                                  semaphoreWaitPatch->pDestination,
                                                                                  semaphoreWaitPatch->offset,
                                                                                  MutableSemaphoreWait::cbEventTimestampSyncWait,
                                                                                  false,
                                                                                  this->semaphore64bCmdSupported);
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
        if (auto *storePatch = std::get_if<PatchTimestampEventPostSyncStoreRegMem>(&cmdToPatch)) {
            auto storeRegMemPtr = std::make_unique<MutableStoreRegisterMemHw<GfxFamily>>(storePatch->pDestination,
                                                                                         storePatch->offset);
            mutableStoreRegMemCmds.emplace_back(std::move(storeRegMemPtr));
            auto storeRegMem = (*mutableStoreRegMemCmds.rbegin()).get();
            variableStoreRegisterMem.emplace_back(storeRegMem);
        }
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
            bool isBufferArg = false;
            if (arg.type == NEO::ArgDescriptor::argTPointer) {
                captureArgument = arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrGlobal ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrConstant ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrUnknown ||
                                  arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal;

                slmArgument = arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal;
                isBufferArg = !slmArgument;
            }
            captureArgument |= immediateArgument;
            if (captureArgument) {
                Variable *variable = nullptr;
                InterfaceVariableDescriptor varDesc = {};

                bool useStageCommit = slmArgument || (isBufferArg && launchParams.isHostSignalScopeEvent && this->l3FlushAfterPostSyncEnabled);

                varDesc.isStageCommit = useStageCommit;
                varDesc.immediateValueChunks = immediateArgument;
                getVariable(&varDesc, &variable);
                variable->setAsKernelArg(kernel->toHandle(), argCount);

                mutableKernelArgumentDesc.kernelArgumentVariable = variable;

                kernelVariables->kernelArguments.push_back(mutableKernelArgumentDesc);

                mutableParams.kernelArgumentMutation = true;

                if (slmArgument) {
                    if (mutableParams.firstSlmArgumentVariable == nullptr) {
                        mutableParams.firstSlmArgumentVariable = variable;
                    }
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
void MutableCommandListCoreFamily<gfxCoreFamily>::storeSignalEventVariable(MutableAppendEvents &mutableEventParams,
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

            mutableEventParams.l3FlushEvent = CommandListCoreFamily<gfxCoreFamily>::compactL3FlushEvent(CommandListCoreFamily<gfxCoreFamily>::getDcFlushRequired(event->isFlushRequiredForSignal()));
            if (CommandList::isInOrderExecutionEnabled()) {
                mutableEventParams.eventInsideInOrder = true;
                mutableEventParams.counterBasedEvent = event->isCounterBased();
                mutableEventParams.inOrderIncrementEvent = event->getInOrderIncrementValue(this->partitionCount) > 0;
                if (mutableEventParams.counterBasedEvent && CommandListCoreFamily<gfxCoreFamily>::duplicatedInOrderCounterStorageEnabled) {
                    mutableEventParams.counterBasedTimestampEvent = event->isEventTimestampFlagSet();
                }
                if (CommandListCoreFamily<gfxCoreFamily>::isInOrderNonWalkerSignalingRequired(event)) {
                    // this command is internally sync by clean StoreDataImm and sync SemWait
                    if (launchParams.outListCommands == nullptr) {
                        this->appendCmdsToPatch.makeCommandView = CommandListCoreFamily<gfxCoreFamily>::isPatchPreambleEnabled();
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
                        this->appendCmdsToPatch.makeCommandView = CommandListCoreFamily<gfxCoreFamily>::isPatchPreambleEnabled();
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
                                         viewKernelAppendLaunchParams.firstSlmArgumentVariable, true);
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
        initialGroupCount,                                            // groupCount
        static_cast<L0::KernelImp *>(viewKernel)->getGroupSize(),     // groupSize
        static_cast<L0::KernelImp *>(viewKernel)->getGlobalOffsets(), // globalOffset
        viewKernel->getPerThreadDataSizeForWholeThreadGroup(),        // perThreadSize
        viewKernel->getRequiredWorkgroupOrder(),                      // walkOrder
        viewKernel->getNumThreadsPerThreadGroup(),                    // numThreadsPerThreadGroup
        viewKernel->getThreadExecutionMask(),                         // threadExecutionMask
        0,                                                            // maxCooperativeGroupCount
        0,                                                            // systemMemoryAllocsCount
        0,                                                            // importedAllocationsCount
        launchParams.requiredPartitionDim,                            // requiredPartitionDim
        launchParams.requiredDispatchWalkOrder,                       // requiredDispatchWalkOrder
        viewKernel->requiresGenerationOfLocalIdsByRuntime(),          // generationOfLocalIdsByRuntime
        launchParams.isCooperative};                                  // cooperativeDispatch

    retVal = addVariableDispatch(viewKernel->getKernelDescriptor(), *viewKernelDispatch,
                                 viewKernelAppendLaunchParams.groupSizeVariable,
                                 viewKernelAppendLaunchParams.groupCountVariable,
                                 viewKernelAppendLaunchParams.globalOffsetVariable,
                                 viewKernelAppendLaunchParams.lastSlmArgumentVariable,
                                 nullptr,
                                 viewKernelMutableComputeWalker, dispatchParams);

    viewKernelAppendLaunchParams.groupCountVariable->resetGroupCountVariable();
    if (viewKernelAppendLaunchParams.groupSizeVariable != nullptr) {
        viewKernelAppendLaunchParams.groupSizeVariable->resetGroupSizeVariable();
    }
    if (viewKernelAppendLaunchParams.globalOffsetVariable != nullptr) {
        viewKernelAppendLaunchParams.globalOffsetVariable->resetGlobalOffsetVariable();
    }

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateCmdListNoopPatchData(size_t noopPatchIndex, void *newCpuPtr, size_t newPatchSize, size_t newOffset, uint64_t newGpuAddress) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    auto &totalNoopSpace = CommandListCoreFamily<gfxCoreFamily>::totalNoopSpace;
    UNRECOVERABLE_IF(noopPatchIndex >= commandsToPatch.size());
    UNRECOVERABLE_IF(!isAligned(newGpuAddress, sizeof(uint64_t)));
    UNRECOVERABLE_IF(!isAligned(newPatchSize, sizeof(uint64_t)));
    auto &noopPatch = std::get<PatchNoopSpace>(commandsToPatch[noopPatchIndex]);

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
    UNRECOVERABLE_IF(!isAligned(newGpuAddress, sizeof(uint64_t)));
    UNRECOVERABLE_IF(!isAligned(newPatchSize, sizeof(uint64_t)));
    size_t noopPatchIndex = commandsToPatch.size();

    totalNoopSpace += newPatchSize;

    commandsToPatch.push_back(PatchNoopSpace{

        .pDestination = newCpuPtr,
        .gpuAddress = newGpuAddress,
        .offset = newOffset,
        .patchSize = newPatchSize,
    });

    return noopPatchIndex;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::fillCmdListNoopPatchData(size_t noopPatchIndex, void *&cpuPtr, size_t &patchSize, size_t &offset, uint64_t &gpuAddress) {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    UNRECOVERABLE_IF(noopPatchIndex >= commandsToPatch.size());
    auto &noopPatch = std::get<PatchNoopSpace>(commandsToPatch[noopPatchIndex]);

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
    auto &noopPatch = std::get<PatchNoopSpace>(commandsToPatch[noopPatchIndex]);

    noopPatch.pDestination = nullptr;

    UNRECOVERABLE_IF(totalNoopSpace < noopPatch.patchSize);
    totalNoopSpace -= noopPatch.patchSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint32_t MutableCommandListCoreFamily<gfxCoreFamily>::getInlineDataSize() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return WalkerType::getInlineDataSize();
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void MutableCommandListCoreFamily<gfxCoreFamily>::storeWaitEventsVariables(uint32_t numWaitEvents,
                                                                                  ze_event_handle_t *phWaitEvents,
                                                                                  MutableAppendEvents &mutableEventParams) {

    if ((this->nextMutationFlags & ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS) == ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS) {
        if (numWaitEvents > 0) {
            AppendEventMutation &currentAppend = this->eventMutations[(nextCommandId - 1)];

            currentAppend.waitEvents.reserve(numWaitEvents);
            mutableEventParams.waitEvents = true;

            for (uint32_t i = 0; i < numWaitEvents; i++) {
                WaitEventVariableDescriptor mutableWaitEventDesc = {};
                Event *event = Event::fromHandle(phWaitEvents[i]);

                Variable *variable = nullptr;
                InterfaceVariableDescriptor varDesc = {};
                varDesc.asyncMutation = CommandListCoreFamily<gfxCoreFamily>::isPatchPreambleEnabled();
                getVariable(&varDesc, &variable);

                variable->setAsWaitEvent(event);

                mutableWaitEventDesc.event = event;
                mutableWaitEventDesc.eventVariable = variable;
                mutableWaitEventDesc.waitEventIndex = i;

                if (CommandList::isInOrderExecutionEnabled() && event->isCounterBased()) {
                    mutableWaitEventDesc.waitEventPackets = event->getInOrderExecEventHelper().getEventData()->devicePartitions;
                    mutableEventParams.omitWaitEventResidency = true;
                    auto deviceCounterAlloc = event->getInOrderExecEventHelper().getDeviceCounterAllocation();
                    addToResidencyContainer(getDeviceCounterAllocForResidency(deviceCounterAlloc));
                    if (event->isExternalEvent()) {
                        auto deviceCounterPatchPreambleAlloc = event->getInOrderExecEventHelper().getPatchPreambleDeviceAllocation();
                        addToResidencyContainer(deviceCounterPatchPreambleAlloc);
                    }
                } else {
                    mutableWaitEventDesc.waitEventPackets = event->getPacketsToWait();
                }
                currentAppend.waitEvents.push_back(mutableWaitEventDesc);

                NEO::GraphicsAllocation *eventPoolAlloc = event->getAllocation(this->device);
                if (eventPoolAlloc) {
                    mutableEventParams.omitWaitEventResidency = true;
                    addToResidencyContainer(eventPoolAlloc);
                }
            }

            this->appendCmdsToPatch.makeCommandView = CommandListCoreFamily<gfxCoreFamily>::isPatchPreambleEnabled();
            mutableEventParams.mutableCmdPatchlistContainer = &this->appendCmdsToPatch;
            CommandListCoreFamily<gfxCoreFamily>::allowCbWaitEventsNoopDispatch = true;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void MutableCommandListCoreFamily<gfxCoreFamily>::processWaitEventVariables(uint32_t numWaitEvents) {
    auto waitEventCmdToPatchIterator = this->appendCmdsToPatch.begin();
    if (auto *cmd = std::get_if<PatchPrefetchKernelMemory>(&(*waitEventCmdToPatchIterator))) {
        waitEventCmdToPatchIterator++;
    }

    AppendEventMutation &currentAppend = this->eventMutations[(nextCommandId - 1)];
    for (uint32_t i = 0; i < numWaitEvents; i++) {
        WaitEventVariableDescriptor &mutableWaitEvent = currentAppend.waitEvents[i];
        UNRECOVERABLE_IF(i != mutableWaitEvent.waitEventIndex);

        auto &variableSemWaitCmdList = mutableWaitEvent.eventVariable->getSemWaitList();
        auto &variableLoadRegImmCmdList = mutableWaitEvent.eventVariable->getLoadRegImmList();

        if (mutableWaitEvent.event->isExternalEvent()) {
            for (uint32_t packet = 0; packet < mutableWaitEvent.event->getInOrderExecEventHelper().getEventData()->devicePartitions; packet++) {
                captureExternalCounterBasedWaitEventCommands(waitEventCmdToPatchIterator, variableSemWaitCmdList, variableLoadRegImmCmdList);
            }
        }

        for (uint32_t packet = 0; packet < mutableWaitEvent.waitEventPackets; packet++) {
            if (CommandList::isInOrderExecutionEnabled() && mutableWaitEvent.event->isCounterBased() && (this->heaplessModeEnabled || !mutableWaitEvent.event->hasInOrderTimestampNode())) {
                captureCounterBasedWaitEventCommands(waitEventCmdToPatchIterator, variableSemWaitCmdList, variableLoadRegImmCmdList);
            } else {
                captureRegularWaitEventCommands(waitEventCmdToPatchIterator, variableSemWaitCmdList);
            }
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void MutableCommandListCoreFamily<gfxCoreFamily>::clearMutableAppendData() {
    this->appendCmdsToPatch.clear();
    this->nextAppendKernelMutable = false;
    this->nextMutationFlags = 0;
    this->appendKernelMutableComputeWalker = nullptr;
    CommandListCoreFamily<gfxCoreFamily>::allowCbWaitEventsNoopDispatch = false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent,
                                                                       uint32_t numWaitEvents,
                                                                       ze_event_handle_t *phWaitEvents,
                                                                       CmdListWaitEventParameters &waitEventsParameters) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, waitEventsParameters);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.waitEvents) {
            processWaitEventVariables(numWaitEvents);
        }

        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(uint32_t numRanges,
                                                                                   const size_t *pRangeSizes,
                                                                                   const void **pRanges,
                                                                                   ze_event_handle_t hSignalEvent,
                                                                                   uint32_t numWaitEvents,
                                                                                   ze_event_handle_t *phWaitEvents,
                                                                                   CmdListWaitEventParameters &waitEventParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        waitEventParams.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        waitEventParams.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents, waitEventParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.waitEvents) {
            processWaitEventVariables(numWaitEvents);
        }

        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage,
                                                                                      const void *srcptr,
                                                                                      const ze_image_region_t *pDstRegion,
                                                                                      uint32_t srcRowPitch,
                                                                                      uint32_t srcSlicePitch,
                                                                                      ze_event_handle_t hEvent,
                                                                                      uint32_t numWaitEvents,
                                                                                      ze_event_handle_t *phWaitEvents,
                                                                                      CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemoryExt(hDstImage, srcptr, pDstRegion, srcRowPitch, srcSlicePitch, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemoryExt(void *dstptr,
                                                                                    ze_image_handle_t hSrcImage,
                                                                                    const ze_image_region_t *pSrcRegion,
                                                                                    uint32_t destRowPitch,
                                                                                    uint32_t destSlicePitch,
                                                                                    ze_event_handle_t hEvent,
                                                                                    uint32_t numWaitEvents,
                                                                                    ze_event_handle_t *phWaitEvents,
                                                                                    CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemoryExt(dstptr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                               ze_image_handle_t hSrcImage,
                                                                               const ze_image_region_t *pDstRegion,
                                                                               const ze_image_region_t *pSrcRegion,
                                                                               ze_event_handle_t hSignalEvent,
                                                                               uint32_t numWaitEvents,
                                                                               ze_event_handle_t *phWaitEvents,
                                                                               CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(void *dstptr,
                                                                          const void *srcptr,
                                                                          size_t size,
                                                                          ze_event_handle_t hSignalEvent,
                                                                          uint32_t numWaitEvents,
                                                                          ze_event_handle_t *phWaitEvents,
                                                                          CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyFromContext(void *dstptr,
                                                                                     ze_context_handle_t hContextSrc,
                                                                                     const void *srcptr,
                                                                                     size_t size,
                                                                                     ze_event_handle_t hSignalEvent,
                                                                                     uint32_t numWaitEvents,
                                                                                     ze_event_handle_t *phWaitEvents,
                                                                                     CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyFromContext(dstptr, hContextSrc, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(void *dstPtr,
                                                                                const ze_copy_region_t *dstRegion,
                                                                                uint32_t dstPitch,
                                                                                uint32_t dstSlicePitch,
                                                                                const void *srcPtr,
                                                                                const ze_copy_region_t *srcRegion,
                                                                                uint32_t srcPitch,
                                                                                uint32_t srcSlicePitch,
                                                                                ze_event_handle_t hSignalEvent,
                                                                                uint32_t numWaitEvents,
                                                                                ze_event_handle_t *phWaitEvents,
                                                                                CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch, srcPtr, srcRegion, srcPitch, srcSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(void *ptr,
                                                                          const void *pattern,
                                                                          size_t patternSize,
                                                                          size_t size,
                                                                          ze_event_handle_t hSignalEvent,
                                                                          uint32_t numWaitEvents,
                                                                          ze_event_handle_t *phWaitEvents,
                                                                          CmdListMemoryCopyParams &memoryCopyParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        memoryCopyParams.waitEventsParameters.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        memoryCopyParams.waitEventsParameters.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        UNRECOVERABLE_IF(!mutableEventParams.waitEvents);
        processWaitEventVariables(numWaitEvents);
        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents,
                                                                            ze_event_handle_t *phEvent,
                                                                            CmdListWaitEventParameters &waitEventParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numEvents, phEvent, mutableEventParams);

        waitEventParams.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        waitEventParams.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phEvent, waitEventParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.waitEvents) {
            processWaitEventVariables(numEvents);
        }

        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(uint64_t *dstptr,
                                                                                    ze_event_handle_t hSignalEvent,
                                                                                    uint32_t numWaitEvents,
                                                                                    ze_event_handle_t *phWaitEvents,
                                                                                    CmdListWaitEventParameters &waitEventParams) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);

        waitEventParams.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        waitEventParams.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents, waitEventParams);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.waitEvents) {
            processWaitEventVariables(numWaitEvents);
        }

        clearMutableAppendData();
    }
    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendHostFunction(ze_host_function_callback_t pHostFunction,
                                                                            void *pUserData,
                                                                            const void *pNext,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents,
                                                                            CmdListHostFunctionParameters &parameters) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    MutableAppendEvents mutableEventParams = {};

    if (this->nextAppendKernelMutable) {
        storeWaitEventsVariables(numWaitEvents, phWaitEvents, mutableEventParams);
        parameters.waitEventParams.skipAddingWaitEventsToResidency = mutableEventParams.omitWaitEventResidency;
        parameters.waitEventParams.outWaitCmds = mutableEventParams.mutableCmdPatchlistContainer;
    }

    result = CommandListCoreFamily<gfxCoreFamily>::appendHostFunction(pHostFunction, pUserData, pNext, hSignalEvent, numWaitEvents, phWaitEvents, parameters);
    if (result != ZE_RESULT_SUCCESS) {
        clearMutableAppendData();
        return result;
    }

    if (this->nextAppendKernelMutable) {
        if (mutableEventParams.waitEvents) {
            processWaitEventVariables(numWaitEvents);
        }

        clearMutableAppendData();
    }
    return result;
}

} // namespace MCL
} // namespace L0
