/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"

#include "encode_surface_state_args.h"
#include "igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    return 4 * MemoryConstants::pageSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isInOrderNonWalkerSignalingRequired(const Event *event) const {
    if (event && compactL3FlushEvent(getDcFlushRequired(event->isSignalScope()))) {
        return true;
    }

    return (!this->duplicatedInOrderCounterStorageEnabled && event && (event->isUsingContextEndOffset() || !event->isCounterBased()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(Kernel *kernel, const ze_group_count_t &threadGroupDimensions, Event *event,
                                                                               CmdListKernelLaunchParams &launchParams) {

    if (NEO::debugManager.flags.ForcePipeControlPriorToWalker.get()) {
        NEO::PipeControlArgs args;
        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
    }
    NEO::Device *neoDevice = device->getNEODevice();
    const auto deviceHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());

    UNRECOVERABLE_IF(kernel == nullptr);
    const auto kernelImmutableData = kernel->getImmutableData();
    auto &kernelDescriptor = kernel->getKernelDescriptor();
    if (kernelDescriptor.kernelAttributes.flags.isInvalid) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        if (kernelImp->checkKernelContainsStatefulAccess()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    if (kernelImp->usesRayTracing()) {
        NEO::GraphicsAllocation *memoryBackedBuffer = device->getNEODevice()->getRTMemoryBackedBuffer();
        if (memoryBackedBuffer == nullptr) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
    }

    setAdditionalKernelLaunchParams(launchParams, *kernel);

    auto kernelInfo = kernelImmutableData->getKernelInfo();

    NEO::IndirectHeap *ssh = nullptr;
    NEO::IndirectHeap *dsh = nullptr;

    DBG_LOG(PrintDispatchParameters, "Kernel: ", kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
            ", Group size: ", kernel->getGroupSize()[0], ", ", kernel->getGroupSize()[1], ", ", kernel->getGroupSize()[2],
            ", Group count: ", threadGroupDimensions.groupCountX, ", ", threadGroupDimensions.groupCountY, ", ", threadGroupDimensions.groupCountZ,
            ", SIMD: ", kernelInfo->getMaxSimdSize());

    bool kernelNeedsImplicitArgs = kernel->getImplicitArgs() != nullptr;
    bool needScratchSpace = false;
    bool kernelNeedsScratchSpace = false;
    if (!launchParams.makeKernelCommandView) {
        for (uint32_t slotId = 0u; slotId < 2; slotId++) {
            auto currentPerThreadScratchSize = kernelDescriptor.kernelAttributes.perThreadScratchSize[slotId];
            if (launchParams.externalPerThreadScratchSize[slotId] > currentPerThreadScratchSize) {
                currentPerThreadScratchSize = launchParams.externalPerThreadScratchSize[slotId];
            }
            if (currentPerThreadScratchSize > commandListPerThreadScratchSize[slotId]) {
                commandListPerThreadScratchSize[slotId] = currentPerThreadScratchSize;
            }
            if (commandListPerThreadScratchSize[slotId] > 0) {
                needScratchSpace = true;
            }
            if (currentPerThreadScratchSize > 0) {
                kernelNeedsScratchSpace = true;
            }
        }
    }
    auto requiredSshSize = kernel->getSurfaceStateHeapDataSize();
    if ((this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) && (requiredSshSize > 0 || needScratchSpace)) {
        if (!this->immediateCmdListHeapSharing && neoDevice->getBindlessHeapsHelper()) {
            commandContainer.prepareBindfulSsh();
            commandContainer.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::surfaceState, requiredSshSize, NEO::EncodeDispatchKernel<GfxFamily>::getDefaultSshAlignment());
        }
    }

    if ((this->immediateCmdListHeapSharing || this->stateBaseAddressTracking) &&
        (this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) &&
        (!launchParams.makeKernelCommandView)) {

        auto &sshReserveConfig = commandContainer.getSurfaceStateHeapReserve();
        NEO::HeapReserveArguments sshReserveArgs = {sshReserveConfig.indirectHeapReservation,
                                                    NEO::EncodeDispatchKernel<GfxFamily>::getSizeRequiredSsh(*kernelInfo),
                                                    NEO::EncodeDispatchKernel<GfxFamily>::getDefaultSshAlignment()};

        // update SSH size - when global bindless addressing is used, kernel args may not require ssh space
        if (kernel->getSurfaceStateHeapDataSize() == 0) {
            sshReserveArgs.size = 0;
        }

        NEO::HeapReserveArguments dshReserveArgs = {};
        if (this->dynamicHeapRequired) {
            auto &dshReserveConfig = commandContainer.getDynamicStateHeapReserve();
            dshReserveArgs = {
                dshReserveConfig.indirectHeapReservation,
                NEO::EncodeDispatchKernel<GfxFamily>::getSizeRequiredDsh(kernelDescriptor, 0),
                NEO::EncodeDispatchKernel<GfxFamily>::getDefaultDshAlignment()};
        }

        commandContainer.reserveSpaceForDispatch(
            sshReserveArgs,
            dshReserveArgs, this->dynamicHeapRequired);

        ssh = sshReserveArgs.indirectHeapReservation;
        dsh = dshReserveArgs.indirectHeapReservation;
    }

    auto kernelPreemptionMode = obtainKernelPreemptionMode(kernel);

    kernel->patchGlobalOffset();
    kernel->patchRegionParams(launchParams);
    this->allocateOrReuseKernelPrivateMemoryIfNeeded(kernel, kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize);

    if (launchParams.isIndirect) {
        prepareIndirectParams(&threadGroupDimensions);
    }
    if (!launchParams.isIndirect) {
        kernel->setGroupCount(threadGroupDimensions.groupCountX,
                              threadGroupDimensions.groupCountY,
                              threadGroupDimensions.groupCountZ);
    }

    uint64_t eventAddress = 0;
    bool isTimestampEvent = false;
    bool l3FlushEnable = false;
    bool isHostSignalScopeEvent = launchParams.isHostSignalScopeEvent;
    bool interruptEvent = false;
    Event *compactEvent = nullptr;
    Event *eventForInOrderExec = event;
    if (event && !launchParams.makeKernelCommandView) {
        if (kernel->getPrintfBufferAllocation() != nullptr) {
            auto module = static_cast<const ModuleImp *>(&static_cast<KernelImp *>(kernel)->getParentModule());
            event->setKernelForPrintf(module->getPrintfKernelWeakPtr(kernel->toHandle()));
            event->setKernelWithPrintfDeviceMutex(kernel->getDevicePrintfKernelMutex());
        }
        isHostSignalScopeEvent = event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST);
        if (compactL3FlushEvent(getDcFlushRequired(event->isSignalScope()))) {
            compactEvent = event;
            event = nullptr;
        } else {
            NEO::GraphicsAllocation *eventPoolAlloc = event->getAllocation(this->device);

            if (eventPoolAlloc) {
                if (!launchParams.omitAddingEventResidency) {
                    commandContainer.addToResidencyContainer(eventPoolAlloc);
                }
                eventAddress = event->getPacketAddress(this->device);
                isTimestampEvent = event->isUsingContextEndOffset();
            }

            bool flushRequired = event->isSignalScope() &&
                                 !launchParams.isKernelSplitOperation;
            l3FlushEnable = getDcFlushRequired(flushRequired);
            interruptEvent = event->isInterruptModeEnabled();
        }
    }

    bool isKernelUsingSystemAllocation = false;
    if (!launchParams.isBuiltInKernel) {
        auto verifyKernelUsingSystemAllocations = [&isKernelUsingSystemAllocation](const NEO::ResidencyContainer &kernelResidencyContainer) {
            for (const auto &allocation : kernelResidencyContainer) {
                if (allocation == nullptr) {
                    continue;
                }
                if (allocation->getAllocationType() == NEO::AllocationType::bufferHostMemory) {
                    isKernelUsingSystemAllocation = true;
                }
            }
        };

        verifyKernelUsingSystemAllocations(kernel->getArgumentsResidencyContainer());
        verifyKernelUsingSystemAllocations(kernel->getInternalResidencyContainer());

    } else {
        isKernelUsingSystemAllocation = launchParams.isDestinationAllocationInSystemMemory;
    }

    if (kernel->hasIndirectAllocationsAllowed()) {
        UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();

        if (unifiedMemoryControls.indirectDeviceAllocationsAllowed) {
            this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
        }
        if (unifiedMemoryControls.indirectHostAllocationsAllowed) {
            this->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
            isKernelUsingSystemAllocation = true;
        }
        if (unifiedMemoryControls.indirectSharedAllocationsAllowed) {
            this->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
        }

        this->indirectAllocationsAllowed = true;
    }

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::KernelNameTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            kernelDescriptor.kernelMetadata.kernelName.c_str(), 0u);
    }

    containsCooperativeKernelsFlag |= launchParams.isCooperative;

    if (!launchParams.makeKernelCommandView) {
        if (kernel->usesSyncBuffer()) {
            auto retVal = (launchParams.isCooperative
                               ? programSyncBuffer(*kernel, *neoDevice, threadGroupDimensions, launchParams.syncBufferPatchIndex)
                               : ZE_RESULT_ERROR_INVALID_ARGUMENT);
            if (retVal) {
                return retVal;
            }
        }

        if (kernel->usesRegionGroupBarrier()) {
            programRegionGroupBarrier(*kernel, threadGroupDimensions, launchParams.localRegionSize, launchParams.regionBarrierPatchIndex);
        }
    }

    bool uncachedMocsKernel = isKernelUncachedMocsRequired(kernelImp->getKernelRequiresUncachedMocs());
    this->requiresQueueUncachedMocs |= kernelImp->getKernelRequiresQueueUncachedMocs();

    if (this->heaplessStateInitEnabled == false && !launchParams.makeKernelCommandView) {
        updateStreamProperties(*kernel, launchParams.isCooperative, threadGroupDimensions, launchParams.isIndirect);
    } else {
        if (!this->isImmediateType()) {
            containsAnyKernel = true;
        }
    }

    auto localMemSize = static_cast<uint32_t>(neoDevice->getDeviceInfo().localMemSize);
    auto slmTotalSize = kernelImp->getSlmTotalSize();
    if (slmTotalSize > 0 && localMemSize < slmTotalSize) {
        CREATE_DEBUG_STRING(str, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        deviceHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    std::list<void *> additionalCommands;

    if (compactEvent && (!compactEvent->isCounterBased() || !this->isImmediateType())) {
        appendEventForProfilingAllWalkers(compactEvent, nullptr, launchParams.outListCommands, true, true, launchParams.omitAddingEventResidency, false);
    }

    bool inOrderExecSignalRequired = false;
    bool inOrderNonWalkerSignalling = false;

    uint64_t inOrderCounterValue = 0;
    NEO::InOrderExecInfo *inOrderExecInfo = nullptr;

    if (!launchParams.makeKernelCommandView) {
        inOrderExecSignalRequired = (this->isInOrderExecutionEnabled() && !launchParams.isKernelSplitOperation && !launchParams.pipeControlSignalling);
        inOrderNonWalkerSignalling = isInOrderNonWalkerSignalingRequired(eventForInOrderExec);

        if (inOrderExecSignalRequired) {
            if (!compactEvent || !this->isImmediateType() || !compactEvent->isCounterBased() || compactEvent->isUsingContextEndOffset()) {
                if (inOrderNonWalkerSignalling) {
                    if (!eventForInOrderExec->getAllocation(this->device) && Event::standaloneInOrderTimestampAllocationEnabled()) {
                        eventForInOrderExec->resetInOrderTimestampNode(device->getInOrderTimestampAllocator()->getTag());
                    }
                    if (!compactEvent || !this->isImmediateType() || !compactEvent->isCounterBased()) {
                        dispatchEventPostSyncOperation(eventForInOrderExec, nullptr, launchParams.outListCommands, Event::STATE_CLEARED, false, false, false, false, false);
                    } else {
                        eventAddress = eventForInOrderExec->getPacketAddress(this->device);
                        isTimestampEvent = true;
                        if (!launchParams.omitAddingEventResidency) {
                            commandContainer.addToResidencyContainer(eventForInOrderExec->getAllocation(this->device));
                        }
                    }
                } else {
                    inOrderCounterValue = this->inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();
                    inOrderExecInfo = this->inOrderExecInfo.get();
                    if (eventForInOrderExec && eventForInOrderExec->isCounterBased() && !isTimestampEvent) {
                        eventAddress = 0;
                    }
                }
            }
        }
    }

    auto maxWgCountPerTile = kernel->getMaxWgCountPerTile(this->engineGroupType);

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        eventAddress,                                           // eventAddress
        static_cast<uint64_t>(Event::STATE_SIGNALED),           // postSyncImmValue
        inOrderCounterValue,                                    // inOrderCounterValue
        neoDevice,                                              // device
        inOrderExecInfo,                                        // inOrderExecInfo
        kernel,                                                 // dispatchInterface
        ssh,                                                    // surfaceStateHeap
        dsh,                                                    // dynamicStateHeap
        reinterpret_cast<const void *>(&threadGroupDimensions), // threadGroupDimensions
        nullptr,                                                // outWalkerPtr
        launchParams.cmdWalkerBuffer,                           // cpuWalkerBuffer
        launchParams.hostPayloadBuffer,                         // cpuPayloadBuffer
        nullptr,                                                // outImplicitArgsPtr
        &additionalCommands,                                    // additionalCommands
        kernelPreemptionMode,                                   // preemptionMode
        launchParams.requiredPartitionDim,                      // requiredPartitionDim
        launchParams.requiredDispatchWalkOrder,                 // requiredDispatchWalkOrder
        launchParams.localRegionSize,                           // localRegionSize
        this->partitionCount,                                   // partitionCount
        launchParams.reserveExtraPayloadSpace,                  // reserveExtraPayloadSpace
        maxWgCountPerTile,                                      // maxWgCountPerTile
        this->defaultPipelinedThreadArbitrationPolicy,          // defaultPipelinedThreadArbitrationPolicy
        launchParams.isIndirect,                                // isIndirect
        launchParams.isPredicate,                               // isPredicate
        isTimestampEvent,                                       // isTimestampEvent
        uncachedMocsKernel,                                     // requiresUncachedMocs
        internalUsage,                                          // isInternal
        launchParams.isCooperative,                             // isCooperative
        isHostSignalScopeEvent,                                 // isHostScopeSignalEvent
        isKernelUsingSystemAllocation,                          // isKernelUsingSystemAllocation
        isImmediateType(),                                      // isKernelDispatchedFromImmediateCmdList
        engineGroupType == NEO::EngineGroupType::renderCompute, // isRcs
        this->dcFlushSupport,                                   // dcFlushEnable
        this->heaplessModeEnabled,                              // isHeaplessModeEnabled
        this->heaplessStateInitEnabled,                         // isHeaplessStateInitEnabled
        interruptEvent,                                         // interruptEvent
        !this->scratchAddressPatchingEnabled,                   // immediateScratchAddressPatching
        launchParams.makeKernelCommandView,                     // makeCommandView
    };

    NEO::EncodeDispatchKernel<GfxFamily>::encodeCommon(commandContainer, dispatchKernelArgs);
    launchParams.outWalker = dispatchKernelArgs.outWalkerPtr;

    if (this->heaplessModeEnabled && this->scratchAddressPatchingEnabled && kernelNeedsScratchSpace) {
        CommandToPatch scratchInlineData;
        scratchInlineData.pDestination = dispatchKernelArgs.outWalkerPtr;
        scratchInlineData.pCommand = nullptr;
        scratchInlineData.type = CommandToPatch::CommandType::ComputeWalkerInlineDataScratch;
        scratchInlineData.offset = NEO::EncodeDispatchKernel<GfxFamily>::getInlineDataOffset(dispatchKernelArgs) +
                                   kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset;
        scratchInlineData.patchSize = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize;
        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (ssh != nullptr) {
            scratchInlineData.baseAddress = ssh->getGpuBase();
        }
        commandsToPatch.push_back(scratchInlineData);

        addPatchScratchAddressInImplicitArgs(commandsToPatch, dispatchKernelArgs, kernelDescriptor, kernelNeedsImplicitArgs);
    }

    if (!this->isFlushTaskSubmissionEnabled) {
        this->containsStatelessUncachedResource = dispatchKernelArgs.requiresUncachedMocs;
    }

    if (!launchParams.makeKernelCommandView) {
        if ((compactEvent && (!compactEvent->isCounterBased() || !this->isImmediateType()))) {
            void **syncCmdBuffer = nullptr;
            if (launchParams.outSyncCommand != nullptr) {
                launchParams.outSyncCommand->type = CommandToPatch::SignalEventPostSyncPipeControl;
                syncCmdBuffer = &launchParams.outSyncCommand->pDestination;
            }
            appendEventForProfilingAllWalkers(compactEvent, syncCmdBuffer, launchParams.outListCommands, false, true, launchParams.omitAddingEventResidency, false);
            if (compactEvent->isInterruptModeEnabled()) {
                NEO::EnodeUserInterrupt<GfxFamily>::encode(*commandContainer.getCommandStream());
            }
        } else if (event) {
            event->setPacketsInUse(partitionCount);
            if (l3FlushEnable) {
                programEventL3Flush(event);
            }
            if (!launchParams.isKernelSplitOperation) {
                dispatchEventRemainingPacketsPostSyncOperation(event, false);
            }
        }
    }

    if (inOrderExecSignalRequired) {
        if (inOrderNonWalkerSignalling) {
            if (!launchParams.skipInOrderNonWalkerSignaling) {
                if ((compactEvent && (compactEvent->isCounterBased() && this->isImmediateType()))) {
                    appendSignalInOrderDependencyCounter(eventForInOrderExec, false, true);
                } else {
                    appendWaitOnSingleEvent(eventForInOrderExec, launchParams.outListCommands, false, false, CommandToPatch::CbEventTimestampPostSyncSemaphoreWait);
                    appendSignalInOrderDependencyCounter(eventForInOrderExec, false, false);
                }
            }
        } else {
            launchParams.skipInOrderNonWalkerSignaling = false;
            UNRECOVERABLE_IF(!dispatchKernelArgs.outWalkerPtr);
            addCmdForPatching(nullptr, dispatchKernelArgs.outWalkerPtr, nullptr, inOrderCounterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::walker);
        }
    } else {
        launchParams.skipInOrderNonWalkerSignaling = false;
    }

    if (neoDevice->getDebugger() && !this->immediateCmdListHeapSharing && !neoDevice->getBindlessHeapsHelper() && this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) {
        auto *ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        auto surfaceStateSpace = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
        auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = &surfaceState;
        args.graphicsAddress = device->getDebugSurface()->getGpuAddress();
        args.size = device->getDebugSurface()->getUnderlyingBufferSize();
        args.mocs = device->getMOCS(false, false);
        args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
        args.allocation = device->getDebugSurface();
        args.gmmHelper = neoDevice->getGmmHelper();
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.implicitScaling = this->partitionCount > 1;
        args.isDebuggerActive = true;

        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
    }
    // Attach kernel residency to our CommandList residency
    {
        if (!launchParams.omitAddingKernelInternalResidency) {
            commandContainer.addToResidencyContainer(kernelImmutableData->getIsaGraphicsAllocation());
            auto &internalResidencyContainer = kernel->getInternalResidencyContainer();
            for (auto resource : internalResidencyContainer) {
                commandContainer.addToResidencyContainer(resource);
            }
        }
        if (!launchParams.omitAddingKernelArgumentResidency) {
            auto &argumentsResidencyContainer = kernel->getArgumentsResidencyContainer();
            for (auto resource : argumentsResidencyContainer) {
                commandContainer.addToResidencyContainer(resource);
            }
        }
    }

    // Store PrintfBuffer from a kernel
    {
        if (kernelDescriptor.kernelAttributes.flags.usesPrintf) {
            storePrintfKernel(kernel);
        }
    }

    if (kernelDescriptor.kernelAttributes.flags.usesAssert) {
        kernelWithAssertAppended = true;
    }

    if (kernelImp->usesRayTracing()) {
        if (this->device->getProductHelper().isDcFlushMitigated()) {
            this->requiresDcFlushForDcMitigation = true;
        }

        NEO::PipeControlArgs args{};
        args.stateCacheInvalidationEnable = true;
        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), neoDevice->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        commandsToPatch.push_back({0x0, additionalCommands.front(), 0, CommandToPatch::PauseOnEnqueuePipeControlStart});
        additionalCommands.pop_front();
        commandsToPatch.push_back({0x0, additionalCommands.front(), 0, CommandToPatch::PauseOnEnqueueSemaphoreStart});
        additionalCommands.pop_front();
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), neoDevice->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        commandsToPatch.push_back({0x0, additionalCommands.front(), 0, CommandToPatch::PauseOnEnqueuePipeControlEnd});
        additionalCommands.pop_front();
        commandsToPatch.push_back({0x0, additionalCommands.front(), 0, CommandToPatch::PauseOnEnqueueSemaphoreEnd});
        additionalCommands.pop_front();
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionPrologue(uint32_t partitionDataSize) {
    NEO::ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(*commandContainer.getCommandStream(),
                                                                    partitionDataSize,
                                                                    isCopyOnly(false));
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionEpilogue() {
    NEO::ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(*commandContainer.getCommandStream(),
                                                                    NEO::ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset(),
                                                                    isCopyOnly(false));
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendComputeBarrierCommand() {
    if (this->partitionCount > 1) {
        auto neoDevice = device->getNEODevice();
        appendMultiTileBarrier(*neoDevice);
    } else {
        NEO::PipeControlArgs args = createBarrierFlags();
        NEO::PostSyncMode postSyncMode = NEO::PostSyncMode::noWrite;
        uint64_t gpuWriteAddress = 0;
        uint64_t writeValue = 0;

        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), postSyncMode, gpuWriteAddress, writeValue, args);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::PipeControlArgs CommandListCoreFamily<gfxCoreFamily>::createBarrierFlags() {
    NEO::PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    return args;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiTileBarrier(NEO::Device &neoDevice) {
    NEO::PipeControlArgs args = createBarrierFlags();
    NEO::ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(*commandContainer.getCommandStream(),
                                                                     neoDevice.getDeviceBitfield(),
                                                                     args,
                                                                     neoDevice.getRootDeviceEnvironment(),
                                                                     0,
                                                                     0,
                                                                     !isImmediateType(),
                                                                     !(this->isFlushTaskSubmissionEnabled || this->dispatchCmdListBatchBufferAsPrimary));
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::estimateBufferSizeMultiTileBarrier(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    return NEO::ImplicitScalingDispatch<GfxFamily>::getBarrierSize(rootDeviceEnvironment, !isImmediateType(), false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(Kernel *kernel,
                                                                          const ze_group_count_t &threadGroupDimensions,
                                                                          Event *event,
                                                                          CmdListKernelLaunchParams &launchParams) {
    if (event) {
        if (eventSignalPipeControl(launchParams.isKernelSplitOperation, getDcFlushRequired(event->isSignalScope()))) {
            event = nullptr;
        } else {
            event->increaseKernelCount();
        }
    }
    return appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendDispatchOffsetRegister(bool workloadPartitionEvent, bool beforeProfilingCmds) {
    if (workloadPartitionEvent && !device->getL0GfxCoreHelper().hasUnifiedPostSyncAllocationLayout()) {
        auto offset = beforeProfilingCmds ? NEO::ImplicitScalingDispatch<GfxFamily>::getTimeStampPostSyncOffset() : NEO::ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset();

        NEO::ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(*commandContainer.getCommandStream(), offset, isCopyOnly(false));
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::singleEventPacketRequired(bool inputSinglePacketEventRequest) const {
    return inputSinglePacketEventRequest;
}

} // namespace L0
