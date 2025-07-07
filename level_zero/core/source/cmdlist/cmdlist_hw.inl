/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/image_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/driver_experimental/zex_cmdlist.h"

#include "CL/cl.h"

#include <algorithm>
#include <unordered_map>

namespace L0 {

inline ze_result_t parseErrorCode(NEO::CommandContainer::ErrorCode returnValue) {
    switch (returnValue) {
    case NEO::CommandContainer::ErrorCode::outOfDeviceMemory:
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    default:
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
CommandListCoreFamily<gfxCoreFamily>::~CommandListCoreFamily() {
    clearCommandsToPatch();
    for (auto &alloc : this->ownedPrivateAllocations) {
        device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(alloc.second);
    }
    this->ownedPrivateAllocations.clear();
    this->storeFillPatternResourcesForReuse();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::postInitComputeSetup() {

    if (!this->stateBaseAddressTracking && !this->heaplessStateInitEnabled) {
        if (!isImmediateType()) {
            programStateBaseAddress(commandContainer, false);
        }
    }

    commandContainer.setDirtyStateForAllHeaps(false);

    setStreamPropertiesDefaultSettings(requiredStreamState);
    setStreamPropertiesDefaultSettings(finalStreamState);

    currentSurfaceStateBaseAddress = NEO::StreamProperty64::initValue;
    currentDynamicStateBaseAddress = NEO::StreamProperty64::initValue;
    currentIndirectObjectBaseAddress = NEO::StreamProperty64::initValue;
    currentBindingTablePoolBaseAddress = NEO::StreamProperty64::initValue;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::reset() {
    this->storeFillPatternResourcesForReuse();
    removeDeallocationContainerData();
    removeHostPtrAllocations();
    removeMemoryPrefetchAllocations();
    commandContainer.reset();
    clearCommandsToPatch();

    if (!isCopyOnly(false)) {
        printfKernelContainer.clear();
        containsStatelessUncachedResource = false;
        indirectAllocationsAllowed = false;
        unifiedMemoryControls.indirectHostAllocationsAllowed = false;
        unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
        unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
        commandListPreemptionMode = device->getDevicePreemptionMode();
        commandListPerThreadScratchSize[0] = 0u;
        commandListPerThreadScratchSize[1] = 0u;
        usedScratchController = nullptr;
        requiredStreamState.resetState();
        finalStreamState.resetState();
        containsAnyKernel = false;
        containsCooperativeKernelsFlag = false;
        commandListSLMEnabled = false;
        kernelWithAssertAppended = false;

        postInitComputeSetup();

        this->returnPoints.clear();
    }

    for (auto &alloc : this->ownedPrivateAllocations) {
        device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(alloc.second);
    }
    this->ownedPrivateAllocations.clear();
    cmdListCurrentStartOffset = 0;

    mappedTsEventList.clear();
    interruptEvents.clear();

    if (inOrderExecInfo) {
        inOrderExecInfo.reset();
        enableInOrderExecution();
    }

    latestOperationRequiredNonWalkerInOrderCmdsChaining = false;
    taskCountUpdateFenceRequired = false;
    textureCacheFlushPending = false;
    closedCmdList = false;

    this->inOrderPatchCmds.clear();

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::assignInOrderExecInfoToEvent(Event *event) {
    event->updateInOrderExecState(inOrderExecInfo, inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::handleInOrderDependencyCounter(Event *signalEvent, bool nonWalkerInOrderCmdsChaining, bool copyOffloadOperation) {
    if (!isInOrderExecutionEnabled()) {
        if (signalEvent && signalEvent->getInOrderExecInfo().get()) {
            UNRECOVERABLE_IF(signalEvent->isCounterBased());
            signalEvent->unsetInOrderExecInfo(); // unset temporary asignment from previous append calls
        }

        return;
    }

    this->handleInOrderCounterOverflow(copyOffloadOperation);

    inOrderExecInfo->addCounterValue(getInOrderIncrementValue());

    this->commandContainer.addToResidencyContainer(inOrderExecInfo->getDeviceCounterAllocation());
    this->commandContainer.addToResidencyContainer(inOrderExecInfo->getHostCounterAllocation());

    if (signalEvent && signalEvent->getInOrderIncrementValue() == 0) {
        if (signalEvent->isCounterBased() || nonWalkerInOrderCmdsChaining || (isImmediateType() && this->duplicatedInOrderCounterStorageEnabled)) {
            assignInOrderExecInfoToEvent(signalEvent);
        } else {
            signalEvent->unsetInOrderExecInfo();
        }
    }

    this->latestOperationRequiredNonWalkerInOrderCmdsChaining = nonWalkerInOrderCmdsChaining;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::handleInOrderCounterOverflow(bool copyOffloadOperation) {
    if (!isQwordInOrderCounter() && ((inOrderExecInfo->getCounterValue() + 1) == std::numeric_limits<uint32_t>::max())) {
        CommandListCoreFamily<gfxCoreFamily>::appendWaitOnInOrderDependency(inOrderExecInfo, nullptr, inOrderExecInfo->getCounterValue() + 1, inOrderExecInfo->getAllocationOffset(), false, true, false, false,
                                                                            isDualStreamCopyOffloadOperation(copyOffloadOperation));

        inOrderExecInfo->resetCounterValue();

        uint32_t newOffset = 0;
        if (inOrderExecInfo->getAllocationOffset() == 0) {
            // multitile immediate writes are uint64_t aligned
            newOffset = this->partitionCount * device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
        }

        inOrderExecInfo->setAllocationOffset(newOffset);
        inOrderExecInfo->initializeAllocationsFromHost();

        CommandListCoreFamily<gfxCoreFamily>::appendSignalInOrderDependencyCounter(nullptr, copyOffloadOperation, false, false); // signal counter on new offset
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::handlePostSubmissionState() {
    this->commandContainer.getResidencyContainer().clear();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::initialize(Device *device, NEO::EngineGroupType engineGroupType,
                                                             ze_command_list_flags_t flags) {
    this->device = device;
    this->commandListPreemptionMode = device->getDevicePreemptionMode();
    this->engineGroupType = engineGroupType;
    this->flags = flags;

    auto &hwInfo = device->getHwInfo();
    auto neoDevice = device->getNEODevice();
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();
    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto releaseHelper = neoDevice->getReleaseHelper();
    auto &l0GfxCoreHelper = device->getL0GfxCoreHelper();
    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    this->dcFlushSupport = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, rootDeviceEnvironment);
    this->systolicModeSupport = NEO::PreambleHelper<GfxFamily>::isSystolicModeConfigurable(rootDeviceEnvironment);
    this->stateComputeModeTracking = L0GfxCoreHelper::enableStateComputeModeTracking(rootDeviceEnvironment);
    this->frontEndStateTracking = L0GfxCoreHelper::enableFrontEndStateTracking(rootDeviceEnvironment);
    this->pipelineSelectStateTracking = L0GfxCoreHelper::enablePipelineSelectStateTracking(rootDeviceEnvironment);
    this->stateBaseAddressTracking = L0GfxCoreHelper::enableStateBaseAddressTracking(rootDeviceEnvironment);
    this->pipeControlMultiKernelEventSync = L0GfxCoreHelper::usePipeControlMultiKernelEventSync(hwInfo);
    this->signalAllEventPackets = L0GfxCoreHelper::useSignalAllEventPackets(hwInfo);
    this->dynamicHeapRequired = NEO::EncodeDispatchKernel<GfxFamily>::isDshNeeded(device->getDeviceInfo());
    this->doubleSbaWa = productHelper.isAdditionalStateBaseAddressWARequired(hwInfo);
    this->defaultMocsIndex = (gmmHelper->getL3EnabledMOCS() >> 1);
    this->l1CachePolicyData.init(productHelper);
    this->cmdListHeapAddressModel = L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment);
    this->dummyBlitWa.rootDeviceEnvironment = &(neoDevice->getRootDeviceEnvironmentRef());
    this->dispatchCmdListBatchBufferAsPrimary = L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, !(this->internalUsage && isImmediateType()));
    this->useOnlyGlobalTimestamps = gfxCoreHelper.useOnlyGlobalTimestamps();
    this->maxFillPaternSizeForCopyEngine = productHelper.getMaxFillPaternSizeForCopyEngine();
    this->heaplessModeEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);
    this->heaplessStateInitEnabled = compilerProductHelper.isHeaplessStateInitEnabled(this->heaplessModeEnabled);
    this->requiredStreamState.initSupport(rootDeviceEnvironment);
    this->finalStreamState.initSupport(rootDeviceEnvironment);
    this->duplicatedInOrderCounterStorageEnabled = gfxCoreHelper.duplicatedInOrderCounterStorageEnabled(rootDeviceEnvironment);
    this->inOrderAtomicSignalingEnabled = gfxCoreHelper.inOrderAtomicSignallingEnabled(rootDeviceEnvironment);
    this->scratchAddressPatchingEnabled = (this->heaplessModeEnabled && !isImmediateType());
    this->copyOperationFenceSupported = (isCopyOnly(false) || isCopyOffloadEnabled()) && productHelper.isDeviceToHostCopySignalingFenceRequired();
    this->defaultPipelinedThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    this->implicitSynchronizedDispatchForCooperativeKernelsAllowed = l0GfxCoreHelper.implicitSynchronizedDispatchForCooperativeKernelsAllowed();
    this->maxLocalSubRegionSize = productHelper.getMaxLocalSubRegionSize(hwInfo);
    this->l3FlushAfterPostSyncRequired = productHelper.isL3FlushAfterPostSyncRequired(heaplessModeEnabled);
    this->compactL3FlushEventPacket = L0GfxCoreHelper::useCompactL3FlushEventPacket(hwInfo, this->l3FlushAfterPostSyncRequired);
    this->useAdditionalBlitProperties = productHelper.useAdditionalBlitProperties();
    this->isPostImageWriteFlushRequired = releaseHelper ? releaseHelper->isPostImageWriteFlushRequired() : false;

    if (NEO::debugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        this->defaultPipelinedThreadArbitrationPolicy = NEO::debugManager.flags.OverrideThreadArbitrationPolicy.get();
    }
    this->statelessBuiltinsEnabled = compilerProductHelper.isForceToStatelessRequired();
    this->localDispatchSupport = productHelper.getSupportedLocalDispatchSizes(hwInfo).size() > 0;

    this->commandContainer.doubleSbaWaRef() = this->doubleSbaWa;
    this->commandContainer.l1CachePolicyDataRef() = &this->l1CachePolicyData;
    this->commandContainer.setHeapAddressModel(this->cmdListHeapAddressModel);
    if (isImmediateType()) {
        this->commandContainer.setImmediateCmdListCsr(getCsr(false));
    }
    this->commandContainer.setStateBaseAddressTracking(this->stateBaseAddressTracking);
    this->commandContainer.setUsingPrimaryBuffer(this->dispatchCmdListBatchBufferAsPrimary);

    if (device->isImplicitScalingCapable() && !this->internalUsage && !isCopyOnly(false)) {
        this->partitionCount = static_cast<uint32_t>(neoDevice->getDeviceBitfield().count());
    }

    if (isImmediateType()) {
        commandContainer.setFlushTaskUsedForImmediate(true);
        commandContainer.setNumIddPerBlock(1);
        this->setupFlushMethod(rootDeviceEnvironment);
    }

    if (this->immediateCmdListHeapSharing) {
        commandContainer.enableHeapSharing();
    }

    commandContainer.setReservedSshSize(getReserveSshSize());
    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);

    auto createSecondaryCmdBufferInHostMem = isImmediateType() &&
                                             !device->isImplicitScalingCapable() &&
                                             getCsr(false) &&
                                             getCsr(false)->isAnyDirectSubmissionEnabled() &&
                                             !neoDevice->getExecutionEnvironment()->areMetricsEnabled() &&
                                             neoDevice->getMemoryManager()->isLocalMemorySupported(neoDevice->getRootDeviceIndex());

    if (NEO::debugManager.flags.DirectSubmissionFlatRingBuffer.get() != -1) {
        createSecondaryCmdBufferInHostMem = isImmediateType() && static_cast<bool>(NEO::debugManager.flags.DirectSubmissionFlatRingBuffer.get());
    }

    auto returnValue = commandContainer.initialize(deviceImp->getActiveDevice(),
                                                   deviceImp->allocationsForReuse.get(),
                                                   NEO::EncodeStates<GfxFamily>::getSshHeapSize(),
                                                   !isCopyOnly(false),
                                                   createSecondaryCmdBufferInHostMem);
    if (!this->pipelineSelectStateTracking) {
        // allow systolic support set in container when tracking disabled
        // setting systolic support allows dispatching untracked command in legacy mode
        commandContainer.systolicModeSupportRef() = this->systolicModeSupport;
    }

    ze_result_t returnType = parseErrorCode(returnValue);
    if (returnType == ZE_RESULT_SUCCESS) {
        if (!isCopyOnly(false)) {
            postInitComputeSetup();
        }
    }

    if (this->flags & ZE_COMMAND_LIST_FLAG_IN_ORDER) {
        enableInOrderExecution();
    }

    if (NEO::debugManager.flags.ForceSynchronizedDispatchMode.get() != -1) {
        enableSynchronizedDispatch((NEO::debugManager.flags.ForceSynchronizedDispatchMode.get() == 1) ? NEO::SynchronizedDispatchMode::full : NEO::SynchronizedDispatchMode::disabled);
    }

    const bool copyOffloadSupported = l0GfxCoreHelper.isDefaultCmdListWithCopyOffloadSupported(this->useAdditionalBlitProperties);

    if (copyOffloadSupported && !this->internalUsage && !isCopyOffloadEnabled()) {
        enableCopyOperationOffload();
    }

    return returnType;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::close() {
    commandContainer.removeDuplicatesFromResidencyContainer();
    if (this->dispatchCmdListBatchBufferAsPrimary) {
        commandContainer.endAlignedPrimaryBuffer();
    } else {
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferEnd(commandContainer);
    }
    closedCmdList = true;

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programL3(bool isSLMused) {}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::prefetchKernelMemory(NEO::LinearStream &cmdStream, const Kernel &kernel, const NEO::GraphicsAllocation *iohAllocation, size_t iohOffset, CommandToPatchContainer *outListCommands, uint64_t cmdId) {
    if (!kernelMemoryPrefetchEnabled()) {
        return;
    }

    auto &rootExecEnv = device->getNEODevice()->getRootDeviceEnvironment();

    auto currentCmdStreamPtr = cmdStream.getSpace(0);
    auto cmdStreamOffset = cmdStream.getUsed();

    NEO::EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(cmdStream, *iohAllocation, kernel.getIndirectSize(), iohOffset, rootExecEnv);
    addKernelIndirectDataMemoryPrefetchPadding(cmdStream, kernel, cmdId);

    NEO::EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(cmdStream, *kernel.getIsaAllocation(), kernel.getImmutableData()->getIsaSize(), kernel.getIsaOffsetInParentAllocation(), rootExecEnv);
    addKernelIsaMemoryPrefetchPadding(cmdStream, kernel, cmdId);

    if (outListCommands) {
        auto &prefetchToPatch = outListCommands->emplace_back();
        prefetchToPatch.type = CommandToPatch::PrefetchKernelMemory;
        prefetchToPatch.pDestination = currentCmdStreamPtr;
        prefetchToPatch.patchSize = cmdStream.getUsed() - cmdStreamOffset;
        prefetchToPatch.offset = iohOffset;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint32_t CommandListCoreFamily<gfxCoreFamily>::getIohSizeForPrefetch(const Kernel &kernel, uint32_t reserveExtraSpace) const {
    return kernel.getIndirectSize() + reserveExtraSpace;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                                                     const ze_group_count_t &threadGroupDimensions,
                                                                     ze_event_handle_t hEvent,
                                                                     uint32_t numWaitEvents,
                                                                     ze_event_handle_t *phWaitEvents,
                                                                     CmdListKernelLaunchParams &launchParams) {

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            launchParams.isCooperative ? "zeCommandListAppendLaunchCooperativeKernel" : "zeCommandListAppendLaunchKernel",
            callId);
    }

    auto kernel = Kernel::fromHandle(kernelHandle);

    auto result = validateLaunchParams(*kernel, launchParams);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto ioh = commandContainer.getHeapWithRequiredSizeAndAlignment(NEO::IndirectHeapType::indirectObject, getIohSizeForPrefetch(*kernel, launchParams.reserveExtraPayloadSpace), GfxFamily::indirectDataAlignment);

    ensureCmdBufferSpaceForPrefetch();
    prefetchKernelMemory(*commandContainer.getCommandStream(), *kernel, ioh->getGraphicsAllocation(), ioh->getUsed(), launchParams.outListCommands, getPrefetchCmdId());

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, launchParams.outListCommands, launchParams.relaxedOrderingDispatch, true, true, launchParams.omitAddingWaitEventsResidency, false);
    if (ret) {
        return ret;
    }

    if (launchParams.isCooperative && this->implicitSynchronizedDispatchForCooperativeKernelsAllowed) {
        enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::full);
    }
    if (this->synchronizedDispatchMode != NEO::SynchronizedDispatchMode::disabled) {
        appendSynchronizedDispatchInitializationSection();
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
        if (!launchParams.isKernelSplitOperation) {
            event->resetKernelCountAndPacketUsedCount();
        }
    }

    if (!handleCounterBasedEventOperations(event, launchParams.omitAddingEventResidency)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto res = appendLaunchKernelWithParams(kernel, threadGroupDimensions, event, launchParams);

    if (!launchParams.skipInOrderNonWalkerSignaling) {
        handleInOrderDependencyCounter(event, isInOrderNonWalkerSignalingRequired(event) && !(event && event->isCounterBased() && event->isUsingContextEndOffset()), false);
    }

    if (this->synchronizedDispatchMode != NEO::SynchronizedDispatchMode::disabled) {
        appendSynchronizedDispatchCleanupSection();
    }

    addToMappedEventList(event);
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            launchParams.isCooperative ? "zeCommandListAppendLaunchCooperativeKernel" : "zeCommandListAppendLaunchKernel",
            callId);
    }

    return res;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                                                             const ze_group_count_t &pDispatchArgumentsBuffer,
                                                                             ze_event_handle_t hEvent,
                                                                             uint32_t numWaitEvents,
                                                                             ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, relaxedOrderingDispatch, true, true, false, false);
    if (ret) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    CmdListKernelLaunchParams launchParams = {};
    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
        if (Kernel::fromHandle(kernelHandle)->getPrintfBufferAllocation() != nullptr) {
            Kernel *kernel = Kernel::fromHandle(kernelHandle);
            auto module = static_cast<const ModuleImp *>(&static_cast<KernelImp *>(kernel)->getParentModule());
            event->setKernelForPrintf(module->getPrintfKernelWeakPtr(kernelHandle));
            event->setKernelWithPrintfDeviceMutex(kernel->getDevicePrintfKernelMutex());
        }
        launchParams.isHostSignalScopeEvent = event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST);
    }

    if (!handleCounterBasedEventOperations(event, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(event, nullptr, true, false, false, false);
    launchParams.isIndirect = true;
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    ret = appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandle), pDispatchArgumentsBuffer,
                                       nullptr, launchParams);
    addToMappedEventList(event);
    appendSignalEventPostWalker(event, nullptr, nullptr, false, false, false);

    handleInOrderDependencyCounter(event, isInOrderNonWalkerSignalingRequired(event), false);

    appendSynchronizedDispatchCleanupSection();

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                                                      const ze_kernel_handle_t *kernelHandles,
                                                                                      const uint32_t *pNumLaunchArguments,
                                                                                      const ze_group_count_t *pLaunchArgumentsBuffer,
                                                                                      ze_event_handle_t hEvent,
                                                                                      uint32_t numWaitEvents,
                                                                                      ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, relaxedOrderingDispatch, true, true, false, false);
    if (ret) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isIndirect = true;
    launchParams.isPredicate = true;
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
        launchParams.isHostSignalScopeEvent = event->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST);
    }

    if (!handleCounterBasedEventOperations(event, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(event, nullptr, true, false, false, false);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(static_cast<const void *>(pNumLaunchArguments));
    auto alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    commandContainer.addToResidencyContainer(alloc);

    for (uint32_t i = 0; i < numKernels; i++) {
        NEO::EncodeMathMMIO<GfxFamily>::encodeGreaterThanPredicate(commandContainer, alloc->getGpuAddress(), i, isCopyOnly(false));

        ret = appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandles[i]),
                                           pLaunchArgumentsBuffer[i],
                                           nullptr, launchParams);
        if (ret) {
            return ret;
        }
    }
    addToMappedEventList(event);
    appendSignalEventPostWalker(event, nullptr, nullptr, false, false, false);

    appendSynchronizedDispatchCleanupSection();

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithArguments(ze_kernel_handle_t hKernel,
                                                                                  const ze_group_count_t groupCounts,
                                                                                  const ze_group_size_t groupSizes,

                                                                                  void **pArguments,
                                                                                  void *pNext,
                                                                                  ze_event_handle_t hSignalEvent,
                                                                                  uint32_t numWaitEvents,
                                                                                  ze_event_handle_t *phWaitEvents) {

    auto kernel = L0::Kernel::fromHandle(hKernel);

    if (kernel == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto result = kernel->setGroupSize(groupSizes.groupSizeX, groupSizes.groupSizeY, groupSizes.groupSizeZ);

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto &args = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs;

    if (args.size() > 0 && !pArguments) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    for (auto i = 0u; i < args.size(); i++) {

        auto &arg = args[i];
        auto argSize = sizeof(void *);
        auto argValue = pArguments[i];

        switch (arg.type) {
        case NEO::ArgDescriptor::argTPointer:
            if (arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal) {
                argSize = *reinterpret_cast<size_t *>(argValue);
                argValue = nullptr;
            }
            break;
        case NEO::ArgDescriptor::argTValue:
            argSize = std::numeric_limits<size_t>::max();

        default:
            break;
        }
        result = kernel->setArgumentValue(i, argSize, argValue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    L0::CmdListKernelLaunchParams launchParams = {};
    launchParams.skipInOrderNonWalkerSignaling = this->skipInOrderNonWalkerSignalingAllowed(hSignalEvent);

    return this->appendLaunchKernel(hKernel, groupCounts, hSignalEvent, numWaitEvents, phWaitEvents, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendEventReset(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);

    event->disableImplicitCounterBasedMode();

    if (event->isCounterBased()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendEventReset",
            callId);
    }

    if (this->isInOrderExecutionEnabled()) {
        handleInOrderImplicitDependencies(isRelaxedOrderingDispatchAllowed(0, false), false);
    }

    appendSynchronizedDispatchInitializationSection();

    event->resetPackets(false);
    event->disableHostCaching(!isImmediateType());
    commandContainer.addToResidencyContainer(event->getAllocation(this->device));

    // default state of event is single packet, handle case when reset is used 1st, launchkernel 2nd - just reset all packets then, use max
    bool useMaxPackets = event->isEventTimestampFlagSet() || (event->getPacketsInUse() < this->partitionCount);

    bool appendPipeControlWithPostSync = (!isCopyOnly(false)) && (event->isSignalScope() || event->isEventTimestampFlagSet());
    dispatchEventPostSyncOperation(event, nullptr, nullptr, Event::STATE_CLEARED, false, useMaxPackets, appendPipeControlWithPostSync, false, isCopyOnly(false));

    if (!isCopyOnly(false)) {
        if (this->partitionCount > 1) {
            appendMultiTileBarrier(*neoDevice);
        }
    }

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(event, false, false, false);
    }
    handleInOrderDependencyCounter(event, false, false);
    event->unsetInOrderExecInfo();

    appendSynchronizedDispatchCleanupSection();

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendEventReset",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(uint32_t numRanges,
                                                                            const size_t *pRangeSizes,
                                                                            const void **pRanges,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, false, true, true, false, false);
    if (ret) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(signalEvent, nullptr, true, false, false, isCopyOnly(false));
    applyMemoryRangesBarrier(numRanges, pRangeSizes, pRanges);
    appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, isCopyOnly(false));
    addToMappedEventList(signalEvent);

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
    }
    handleInOrderDependencyCounter(signalEvent, false, false);

    appendSynchronizedDispatchCleanupSection();

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(ze_image_handle_t hDstImage, const void *srcPtr, const ze_image_region_t *pDstRegion, ze_event_handle_t hEvent,
                                                                            uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    return appendImageCopyFromMemoryExt(hDstImage, srcPtr, pDstRegion, 0, 0,
                                        hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

static ze_image_region_t getRegionFromImageDesc(ze_image_desc_t imgDesc) {
    ze_image_region_t region{0,
                             0,
                             0,
                             static_cast<uint32_t>(imgDesc.width),
                             static_cast<uint32_t>(imgDesc.height),
                             static_cast<uint32_t>(imgDesc.depth)};
    // If this is a 1D or 2D image, then the height or depth is ignored and must be set to 1.
    // Internally, all dimensions must be >= 1.
    if (imgDesc.type == ZE_IMAGE_TYPE_1D) {
        region.height = 1;
    } else if (imgDesc.type == ZE_IMAGE_TYPE_1DARRAY) {
        region.height = imgDesc.arraylevels;
    }
    if (imgDesc.type == ZE_IMAGE_TYPE_2DARRAY) {
        region.depth = imgDesc.arraylevels;
    } else if (imgDesc.type != ZE_IMAGE_TYPE_3D) {
        region.depth = 1;
    }
    return region;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemoryExt(ze_image_handle_t hDstImage, const void *srcPtr, const ze_image_region_t *pDstRegion, uint32_t srcRowPitch, uint32_t srcSlicePitch,
                                                                               ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    if (!hDstImage) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (!srcPtr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto image = Image::fromHandle(hDstImage);
    auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->imageElementSizeInBytes);
    Vec3<size_t> imgSize = {image->getImageDesc().width,
                            image->getImageDesc().height,
                            image->getImageDesc().depth};
    if (image->getImageDesc().type == ZE_IMAGE_TYPE_1DARRAY) {
        imgSize.y = image->getImageDesc().arraylevels;
    }
    if (image->getImageDesc().type == ZE_IMAGE_TYPE_2DARRAY) {
        imgSize.z = image->getImageDesc().arraylevels;
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    ze_image_region_t tmpRegion;
    if (pDstRegion == nullptr) {
        tmpRegion = getRegionFromImageDesc(image->getImageDesc());
        pDstRegion = &tmpRegion;
    }

    if (srcRowPitch == 0) {
        if (image->isMimickedImage()) {
            uint32_t srcBytesPerPixel = bytesPerPixel;
            if (bytesPerPixel == 8) {
                srcBytesPerPixel = 6;
            }
            if (bytesPerPixel == 4) {
                srcBytesPerPixel = 3;
            }
            srcRowPitch = pDstRegion->width * srcBytesPerPixel;
        } else {
            srcRowPitch = pDstRegion->width * bytesPerPixel;
        }
    }
    if (srcSlicePitch == 0) {
        srcSlicePitch = (image->getImageInfo().imgDesc.imageType == NEO::ImageType::image1DArray ? 1 : pDstRegion->height) * srcRowPitch;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, srcRowPitch, srcSlicePitch, pDstRegion);

    auto allocationStruct = getAlignedAllocationData(this->device, false, srcPtr, bufferSize, true, false);
    if (allocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (driverHandle->isRemoteImageNeeded(image, device)) {
        L0::Image *peerImage = nullptr;

        ze_result_t ret = driverHandle->getPeerImage(device, image, &peerImage);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
        image = peerImage;
    }

    memoryCopyParams.copyOffloadAllowed = isCopyOffloadAllowed(*allocationStruct.alloc, *image->getAllocation());

    if (isCopyOnly(memoryCopyParams.copyOffloadAllowed)) {
        if ((bytesPerPixel == 3) || (bytesPerPixel == 6) || image->isMimickedImage()) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        size_t imgRowPitch = image->getImageInfo().rowPitch;
        size_t imgSlicePitch = image->getImageInfo().slicePitch;
        auto status = appendCopyImageBlit(allocationStruct.alloc, image->getAllocation(),
                                          {0, 0, 0}, {pDstRegion->originX, pDstRegion->originY, pDstRegion->originZ}, srcRowPitch, srcSlicePitch,
                                          imgRowPitch, imgSlicePitch, bytesPerPixel, {pDstRegion->width, pDstRegion->height, pDstRegion->depth}, {pDstRegion->width, pDstRegion->height, pDstRegion->depth}, imgSize,
                                          event, numWaitEvents, phWaitEvents, memoryCopyParams);
        addToMappedEventList(Event::fromHandle(hEvent));
        return status;
    }

    bool isHeaplessEnabled = this->heaplessModeEnabled;
    ImageBuiltin builtInType = ImageBuiltin::copyBufferToImage3dBytes;

    switch (bytesPerPixel) {
    case 1u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3dBytes>(isHeaplessEnabled);
        break;
    case 2u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d2Bytes>(isHeaplessEnabled);
        break;
    case 4u:
        if (image->isMimickedImage()) {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d3To4Bytes>(isHeaplessEnabled);
        } else {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d4Bytes>(isHeaplessEnabled);
        }
        break;
    case 8u:
        if (image->isMimickedImage()) {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d6To8Bytes>(isHeaplessEnabled);
        } else {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d8Bytes>(isHeaplessEnabled);
        }
        break;
    case 16u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyBufferToImage3d16Bytes>(isHeaplessEnabled);
        break;
    default:
        UNRECOVERABLE_IF(true);
        break;
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    Kernel *builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(builtInType);

    builtinKernel->setArgBufferWithAlloc(0u, allocationStruct.alignedAllocationPtr,
                                         allocationStruct.alloc,
                                         nullptr);
    builtinKernel->setArgRedescribedImage(1u, image->toHandle(), false);
    builtinKernel->setArgumentValue(2u, sizeof(size_t), &allocationStruct.offset);

    uint32_t origin[] = {pDstRegion->originX,
                         pDstRegion->originY,
                         pDstRegion->originZ,
                         0};
    builtinKernel->setArgumentValue(3u, sizeof(origin), &origin);

    if (this->heaplessModeEnabled) {
        uint64_t pitch[] = {srcRowPitch, srcSlicePitch};
        builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);
    } else {
        uint32_t pitch[] = {srcRowPitch, srcSlicePitch};
        builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);
    }

    uint32_t groupSizeX = pDstRegion->width;
    uint32_t groupSizeY = pDstRegion->height;
    uint32_t groupSizeZ = pDstRegion->depth;

    ze_result_t ret = builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                                      &groupSizeX, &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    if (pDstRegion->width % groupSizeX || pDstRegion->height % groupSizeY || pDstRegion->depth % groupSizeZ) {
        CREATE_DEBUG_STRING(str, "Invalid group size {%d, %d, %d} specified\n", groupSizeX, groupSizeY, groupSizeZ);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Invalid group size {%d, %d, %d} specified\n",
                           groupSizeX, groupSizeY, groupSizeZ);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t kernelArgs{pDstRegion->width / groupSizeX, pDstRegion->height / groupSizeY,
                                pDstRegion->depth / groupSizeZ};

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.relaxedOrderingDispatch = memoryCopyParams.relaxedOrderingDispatch;

    auto status = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), kernelArgs,
                                                                           event, numWaitEvents, phWaitEvents,
                                                                           launchParams);
    addToMappedEventList(Event::fromHandle(hEvent));

    return status;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(void *dstPtr,
                                                                          ze_image_handle_t hSrcImage,
                                                                          const ze_image_region_t *pSrcRegion,
                                                                          ze_event_handle_t hEvent,
                                                                          uint32_t numWaitEvents,
                                                                          ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    return appendImageCopyToMemoryExt(dstPtr, hSrcImage, pSrcRegion, 0, 0,
                                      hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemoryExt(void *dstPtr,
                                                                             ze_image_handle_t hSrcImage,
                                                                             const ze_image_region_t *pSrcRegion,
                                                                             uint32_t destRowPitch,
                                                                             uint32_t destSlicePitch,
                                                                             ze_event_handle_t hEvent,
                                                                             uint32_t numWaitEvents,
                                                                             ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    if (!dstPtr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if (!hSrcImage) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto image = Image::fromHandle(hSrcImage);
    auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->imageElementSizeInBytes);
    Vec3<size_t> imgSize = {image->getImageDesc().width,
                            image->getImageDesc().height,
                            image->getImageDesc().depth};
    if (image->getImageDesc().type == ZE_IMAGE_TYPE_1DARRAY) {
        imgSize.y = image->getImageDesc().arraylevels;
    }
    if (image->getImageDesc().type == ZE_IMAGE_TYPE_2DARRAY) {
        imgSize.z = image->getImageDesc().arraylevels;
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    ze_image_region_t tmpRegion;
    if (pSrcRegion == nullptr) {
        tmpRegion = getRegionFromImageDesc(image->getImageDesc());
        pSrcRegion = &tmpRegion;
    }

    if (destRowPitch == 0) {
        if (image->isMimickedImage()) {
            uint32_t destBytesPerPixel = bytesPerPixel;
            if (bytesPerPixel == 8) {
                destBytesPerPixel = 6;
            }
            if (bytesPerPixel == 4) {
                destBytesPerPixel = 3;
            }
            destRowPitch = pSrcRegion->width * destBytesPerPixel;
        } else {
            destRowPitch = pSrcRegion->width * bytesPerPixel;
        }
    }
    if (destSlicePitch == 0) {
        destSlicePitch = (image->getImageInfo().imgDesc.imageType == NEO::ImageType::image1DArray ? 1 : pSrcRegion->height) * destRowPitch;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, destRowPitch, destSlicePitch, pSrcRegion);

    auto allocationStruct = getAlignedAllocationData(this->device, false, dstPtr, bufferSize, false, false);
    if (allocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (driverHandle->isRemoteImageNeeded(image, device)) {
        L0::Image *peerImage = nullptr;

        ze_result_t ret = driverHandle->getPeerImage(device, image, &peerImage);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
        image = peerImage;
    }

    memoryCopyParams.copyOffloadAllowed = isCopyOffloadAllowed(*image->getAllocation(), *allocationStruct.alloc);

    if (isCopyOnly(memoryCopyParams.copyOffloadAllowed)) {
        if ((bytesPerPixel == 3) || (bytesPerPixel == 6) || image->isMimickedImage()) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        size_t imgRowPitch = image->getImageInfo().rowPitch;
        size_t imgSlicePitch = image->getImageInfo().slicePitch;
        auto status = appendCopyImageBlit(image->getAllocation(), allocationStruct.alloc,
                                          {pSrcRegion->originX, pSrcRegion->originY, pSrcRegion->originZ}, {0, 0, 0}, imgRowPitch, imgSlicePitch,
                                          destRowPitch, destSlicePitch, bytesPerPixel, {pSrcRegion->width, pSrcRegion->height, pSrcRegion->depth},
                                          imgSize, {pSrcRegion->width, pSrcRegion->height, pSrcRegion->depth}, event, numWaitEvents, phWaitEvents, memoryCopyParams);
        addToMappedEventList(event);
        return status;
    }

    bool isHeaplessEnabled = this->heaplessModeEnabled;
    ImageBuiltin builtInType = ImageBuiltin::copyBufferToImage3dBytes;

    switch (bytesPerPixel) {
    case 1u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBufferBytes>(isHeaplessEnabled);
        break;
    case 2u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer2Bytes>(isHeaplessEnabled);
        break;
    case 3u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer3Bytes>(isHeaplessEnabled);
        break;
    case 4u:
        if (image->isMimickedImage()) {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer4To3Bytes>(isHeaplessEnabled);
        } else {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer4Bytes>(isHeaplessEnabled);
        }
        break;
    case 6u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer6Bytes>(isHeaplessEnabled);
        break;
    case 8u:
        if (image->isMimickedImage()) {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer8To6Bytes>(isHeaplessEnabled);
        } else {
            builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer8Bytes>(isHeaplessEnabled);
        }
        break;
    case 16u:
        builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImage3dToBuffer16Bytes>(isHeaplessEnabled);
        break;
    default: {
        CREATE_DEBUG_STRING(str, "Invalid bytesPerPixel of size: %u\n", bytesPerPixel);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "invalid bytesPerPixel of size: %u\n", bytesPerPixel);
        UNRECOVERABLE_IF(true);
        break;
    }
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    Kernel *builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(builtInType);

    builtinKernel->setArgRedescribedImage(0u, image->toHandle(), false);
    builtinKernel->setArgBufferWithAlloc(1u, allocationStruct.alignedAllocationPtr,
                                         allocationStruct.alloc,
                                         nullptr);
    uint32_t origin[] = {pSrcRegion->originX,
                         pSrcRegion->originY,
                         pSrcRegion->originZ,
                         0};
    builtinKernel->setArgumentValue(2u, sizeof(origin), &origin);
    builtinKernel->setArgumentValue(3u, sizeof(size_t), &allocationStruct.offset);

    if (this->heaplessModeEnabled) {
        uint64_t pitch[] = {destRowPitch, destSlicePitch};
        builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);
    } else {
        uint32_t pitch[] = {destRowPitch, destSlicePitch};
        builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);
    }

    uint32_t groupSizeX = pSrcRegion->width;
    uint32_t groupSizeY = pSrcRegion->height;
    uint32_t groupSizeZ = pSrcRegion->depth;

    ze_result_t ret = builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                                      &groupSizeX, &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    if (pSrcRegion->width % groupSizeX || pSrcRegion->height % groupSizeY || pSrcRegion->depth % groupSizeZ) {
        CREATE_DEBUG_STRING(str, "Invalid group size {%d, %d, %d} specified\n", groupSizeX, groupSizeY, groupSizeZ);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Invalid group size {%d, %d, %d} specified\n",
                           groupSizeX, groupSizeY, groupSizeZ);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (this->device->getNEODevice()->isAnyDirectSubmissionEnabled()) {
        NEO::PipeControlArgs pipeControlArgs;
        pipeControlArgs.textureCacheInvalidationEnable = true;
        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), pipeControlArgs);
    }

    ze_group_count_t kernelArgs{pSrcRegion->width / groupSizeX, pSrcRegion->height / groupSizeY,
                                pSrcRegion->depth / groupSizeZ};

    auto dstAllocationType = allocationStruct.alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::bufferHostMemory) ||
        (dstAllocationType == NEO::AllocationType::externalHostPtr);

    if constexpr (checkIfAllocationImportedRequired()) {
        launchParams.isDestinationAllocationImported = this->isAllocationImported(allocationStruct.alloc, device->getDriverHandle()->getSvmAllocsManager());
    }
    launchParams.relaxedOrderingDispatch = memoryCopyParams.relaxedOrderingDispatch;
    ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), kernelArgs,
                                                                   event, numWaitEvents, phWaitEvents, launchParams);
    addToMappedEventList(event);
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                        ze_image_handle_t hSrcImage,
                                                                        const ze_image_region_t *pDstRegion,
                                                                        const ze_image_region_t *pSrcRegion,
                                                                        ze_event_handle_t hEvent,
                                                                        uint32_t numWaitEvents,
                                                                        ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    auto dstImage = L0::Image::fromHandle(hDstImage);
    auto srcImage = L0::Image::fromHandle(hSrcImage);
    cl_int4 srcOffset, dstOffset;

    ze_image_region_t srcRegion, dstRegion;

    if (pSrcRegion != nullptr) {
        srcRegion = *pSrcRegion;
    } else {
        ze_image_desc_t srcDesc = srcImage->getImageDesc();
        srcRegion = {0, 0, 0, static_cast<uint32_t>(srcDesc.width), srcDesc.height, srcDesc.depth};
    }

    srcOffset.x = static_cast<cl_int>(srcRegion.originX);
    srcOffset.y = static_cast<cl_int>(srcRegion.originY);
    srcOffset.z = static_cast<cl_int>(srcRegion.originZ);
    srcOffset.w = 0;

    if (pDstRegion != nullptr) {
        dstRegion = *pDstRegion;
    } else {
        ze_image_desc_t dstDesc = dstImage->getImageDesc();
        dstRegion = {0, 0, 0, static_cast<uint32_t>(dstDesc.width), dstDesc.height, dstDesc.depth};
    }

    dstOffset.x = static_cast<cl_int>(dstRegion.originX);
    dstOffset.y = static_cast<cl_int>(dstRegion.originY);
    dstOffset.z = static_cast<cl_int>(dstRegion.originZ);
    dstOffset.w = 0;

    if (srcRegion.width != dstRegion.width ||
        srcRegion.height != dstRegion.height ||
        srcRegion.depth != dstRegion.depth) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    uint32_t groupSizeX = srcRegion.width;
    uint32_t groupSizeY = srcRegion.height;
    uint32_t groupSizeZ = srcRegion.depth;

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (driverHandle->isRemoteImageNeeded(dstImage, device)) {
        L0::Image *peerImage = nullptr;

        ze_result_t ret = driverHandle->getPeerImage(device, dstImage, &peerImage);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
        dstImage = peerImage;
    }

    if (driverHandle->isRemoteImageNeeded(srcImage, device)) {
        L0::Image *peerImage = nullptr;

        ze_result_t ret = driverHandle->getPeerImage(device, srcImage, &peerImage);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
        srcImage = peerImage;
    }

    memoryCopyParams.copyOffloadAllowed = isCopyOffloadAllowed(*srcImage->getAllocation(), *dstImage->getAllocation());

    if (isCopyOnly(memoryCopyParams.copyOffloadAllowed)) {
        auto bytesPerPixel = static_cast<uint32_t>(srcImage->getImageInfo().surfaceFormat->imageElementSizeInBytes);

        ze_image_region_t region = getRegionFromImageDesc(srcImage->getImageDesc());
        Vec3<size_t> srcImgSize = {region.width, region.height, region.depth};

        region = getRegionFromImageDesc(dstImage->getImageDesc());
        Vec3<size_t> dstImgSize = {region.width, region.height, region.depth};

        auto srcRowPitch = srcImage->getImageInfo().rowPitch;
        auto srcSlicePitch =
            (srcImage->getImageInfo().imgDesc.imageType == NEO::ImageType::image1DArray ? 1 : srcRegion.height) * srcRowPitch;

        auto dstRowPitch = dstImage->getImageInfo().rowPitch;
        auto dstSlicePitch =
            (dstImage->getImageInfo().imgDesc.imageType == NEO::ImageType::image1DArray ? 1 : dstRegion.height) * dstRowPitch;

        auto status = appendCopyImageBlit(srcImage->getAllocation(), dstImage->getAllocation(),
                                          {srcRegion.originX, srcRegion.originY, srcRegion.originZ}, {dstRegion.originX, dstRegion.originY, dstRegion.originZ}, srcRowPitch, srcSlicePitch,
                                          dstRowPitch, dstSlicePitch, bytesPerPixel, {srcRegion.width, srcRegion.height, srcRegion.depth}, srcImgSize, dstImgSize,
                                          event, numWaitEvents, phWaitEvents, memoryCopyParams);
        addToMappedEventList(event);
        return status;
    }

    ImageBuiltin builtInType = BuiltinTypeHelper::adjustImageBuiltinType<ImageBuiltin::copyImageRegion>(this->heaplessModeEnabled);

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(builtInType);

    ze_result_t ret = kernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX,
                                               &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = kernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    if (srcRegion.width % groupSizeX || srcRegion.height % groupSizeY || srcRegion.depth % groupSizeZ) {
        CREATE_DEBUG_STRING(str, "Invalid group size {%d, %d, %d} specified\n", groupSizeX, groupSizeY, groupSizeZ);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Invalid group size {%d, %d, %d} specified\n",
                           groupSizeX, groupSizeY, groupSizeZ);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t kernelArgs{srcRegion.width / groupSizeX, srcRegion.height / groupSizeY,
                                srcRegion.depth / groupSizeZ};

    const bool isPackedFormat = NEO::ImageHelper::areImagesCompatibleWithPackedFormat(device->getProductHelper(), srcImage->getImageInfo(), dstImage->getImageInfo(), srcImage->getAllocation(), dstImage->getAllocation(), srcRegion.width);

    kernel->setArgRedescribedImage(0, srcImage->toHandle(), isPackedFormat);
    kernel->setArgRedescribedImage(1, dstImage->toHandle(), isPackedFormat);

    kernel->setArgumentValue(2, sizeof(srcOffset), &srcOffset);
    kernel->setArgumentValue(3, sizeof(dstOffset), &dstOffset);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.relaxedOrderingDispatch = memoryCopyParams.relaxedOrderingDispatch;
    auto status = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernel->toHandle(), kernelArgs,
                                                                           event, numWaitEvents, phWaitEvents,
                                                                           launchParams);
    addToMappedEventList(event);

    return status;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopy(ze_image_handle_t hDstImage,
                                                                  ze_image_handle_t hSrcImage,
                                                                  ze_event_handle_t hEvent,
                                                                  uint32_t numWaitEvents,
                                                                  ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {

    return this->appendImageCopyRegion(hDstImage, hSrcImage, nullptr, nullptr, hEvent,
                                       numWaitEvents, phWaitEvents, memoryCopyParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemAdvise(ze_device_handle_t hDevice,
                                                                  const void *ptr, size_t size,
                                                                  ze_memory_advice_t advice) {

    if (ptr == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->memAdviseOperations.push_back(MemAdviseOperation(hDevice, ptr, size, advice));

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::executeMemAdvise(ze_device_handle_t hDevice,
                                                                   const void *ptr, size_t size,
                                                                   ze_memory_advice_t advice) {

    auto driverHandle = device->getDriverHandle();
    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);

    if (!allocData) {
        if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
            NEO::MemAdvise memAdviseOp = NEO::MemAdvise::invalidAdvise;

            switch (advice) {
            case ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION:
                memAdviseOp = NEO::MemAdvise::setPreferredLocation;
                break;
            case ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION:
                memAdviseOp = NEO::MemAdvise::clearPreferredLocation;
                break;
            case ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION:
                memAdviseOp = NEO::MemAdvise::setSystemMemoryPreferredLocation;
                break;
            case ZE_MEMORY_ADVICE_CLEAR_SYSTEM_MEMORY_PREFERRED_LOCATION:
                memAdviseOp = NEO::MemAdvise::clearSystemMemoryPreferredLocation;
                break;
            case ZE_MEMORY_ADVICE_SET_READ_MOSTLY:
            case ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY:
            case ZE_MEMORY_ADVICE_BIAS_CACHED:
            case ZE_MEMORY_ADVICE_BIAS_UNCACHED:
            case ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY:
            case ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY:
            default:
                return ZE_RESULT_SUCCESS;
            }

            DeviceImp *deviceImp = static_cast<DeviceImp *>((L0::Device::fromHandle(hDevice)));
            auto unifiedMemoryManager = driverHandle->getSvmAllocsManager();

            unifiedMemoryManager->sharedSystemMemAdvise(*deviceImp->getNEODevice(), memAdviseOp, ptr, size);

            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    NEO::MemAdviseFlags flags{};
    DeviceImp *deviceImp = static_cast<DeviceImp *>((L0::Device::fromHandle(hDevice)));

    if (deviceImp->memAdviseSharedAllocations.find(allocData) != deviceImp->memAdviseSharedAllocations.end()) {
        flags = deviceImp->memAdviseSharedAllocations[allocData];
    }

    switch (advice) {
    case ZE_MEMORY_ADVICE_SET_READ_MOSTLY:
        flags.readOnly = 1;
        break;
    case ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY:
        flags.readOnly = 0;
        break;
    case ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION:
        flags.devicePreferredLocation = 1;
        break;
    case ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION:
        flags.devicePreferredLocation = 0;
        break;
    case ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION:
        flags.systemPreferredLocation = 1;
        break;
    case ZE_MEMORY_ADVICE_CLEAR_SYSTEM_MEMORY_PREFERRED_LOCATION:
        flags.systemPreferredLocation = 0;
        break;
    case ZE_MEMORY_ADVICE_BIAS_CACHED:
        flags.cachedMemory = 1;
        break;
    case ZE_MEMORY_ADVICE_BIAS_UNCACHED:
        flags.cachedMemory = 0;
        break;
    case ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY:
    case ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY:
    default:
        break;
    }

    auto memoryManager = device->getDriverHandle()->getMemoryManager();
    auto pageFaultManager = memoryManager->getPageFaultManager();

    if (pageFaultManager) {
        /* If Read Only and Device Preferred Hints have been cleared, then cpu_migration of Shared memory can be re-enabled*/
        if (flags.cpuMigrationBlocked) {
            if (flags.readOnly == 0 && flags.devicePreferredLocation == 0) {
                pageFaultManager->protectCPUMemoryAccess(const_cast<void *>(ptr), size);
                flags.cpuMigrationBlocked = 0;
            }
        }
        /* Given MemAdvise hints, use different gpu Domain Handler for the Page Fault Handling */
        pageFaultManager->setGpuDomainHandler(L0::transferAndUnprotectMemoryWithHints);
    }

    auto alloc = allocData->gpuAllocations.getGraphicsAllocation(deviceImp->getRootDeviceIndex());
    memoryManager->setMemAdvise(alloc, flags, deviceImp->getRootDeviceIndex());

    deviceImp->memAdviseSharedAllocations[allocData] = flags;

    return ZE_RESULT_SUCCESS;
}

static inline void builtinSetArgCopy(Kernel *builtinKernel, uint32_t argIndex, void *argPtr, NEO::GraphicsAllocation *allocation) {
    if (allocation) {
        builtinKernel->setArgBufferWithAlloc(argIndex, *reinterpret_cast<uintptr_t *>(argPtr), allocation, nullptr);
    } else {
        builtinKernel->setArgumentValue(argIndex, sizeof(uintptr_t *), argPtr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernelWithGA(void *dstPtr,
                                                                               NEO::GraphicsAllocation *dstPtrAlloc,
                                                                               uint64_t dstOffset,
                                                                               void *srcPtr,
                                                                               NEO::GraphicsAllocation *srcPtrAlloc,
                                                                               uint64_t srcOffset,
                                                                               uint64_t size,
                                                                               uint64_t elementSize,
                                                                               Builtin builtin,
                                                                               Event *signalEvent,
                                                                               bool isStateless,
                                                                               CmdListKernelLaunchParams &launchParams) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    Kernel *builtinKernel = nullptr;

    builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = builtinKernel->getImmutableData()
                              ->getDescriptor()
                              .kernelAttributes.simdSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    ze_result_t ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    builtinSetArgCopy(builtinKernel, 0, dstPtr, dstPtrAlloc);
    builtinSetArgCopy(builtinKernel, 1, srcPtr, srcPtrAlloc);

    uint64_t elems = size / elementSize;
    builtinKernel->setArgumentValue(2, sizeof(elems), &elems);
    builtinKernel->setArgumentValue(3, sizeof(dstOffset), &dstOffset);
    builtinKernel->setArgumentValue(4, sizeof(srcOffset), &srcOffset);

    uint32_t groups = static_cast<uint32_t>((size + ((static_cast<uint64_t>(groupSizeX) * elementSize) - 1)) / (static_cast<uint64_t>(groupSizeX) * elementSize));
    ze_group_count_t dispatchKernelArgs{groups, 1u, 1u};

    launchParams.isBuiltInKernel = true;
    if (dstPtrAlloc) {
        auto dstAllocationType = dstPtrAlloc->getAllocationType();
        launchParams.isDestinationAllocationInSystemMemory = this->isUsingSystemAllocation(dstAllocationType);
        if constexpr (checkIfAllocationImportedRequired()) {
            launchParams.isDestinationAllocationImported = this->isAllocationImported(dstPtrAlloc, device->getDriverHandle()->getSvmAllocsManager());
        }
    } else {
        launchParams.isDestinationAllocationInSystemMemory = true;
    }
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(builtinKernel, dispatchKernelArgs, signalEvent, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isHighPriorityImmediateCmdList() const {
    return (this->isImmediateType() && getCsr(false)->getOsContext().isHighPriority());
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlit(uintptr_t dstPtr,
                                                                       NEO::GraphicsAllocation *dstPtrAlloc,
                                                                       uint64_t dstOffset, uintptr_t srcPtr,
                                                                       NEO::GraphicsAllocation *srcPtrAlloc,
                                                                       uint64_t srcOffset,
                                                                       uint64_t size,
                                                                       Event *signalEvent) {
    if (dstPtrAlloc) {
        dstOffset += ptrDiff<uintptr_t>(dstPtr, dstPtrAlloc->getGpuAddress());
    }
    if (srcPtrAlloc) {
        srcOffset += ptrDiff<uintptr_t>(srcPtr, srcPtrAlloc->getGpuAddress());
    }

    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();
    auto blitProperties = NEO::BlitProperties::constructPropertiesForSystemCopy(dstPtrAlloc, srcPtrAlloc, dstPtr, srcPtr, {dstOffset, 0, 0}, {srcOffset, 0, 0}, {size, 0, 0}, 0, 0, 0, 0, clearColorAllocation);
    blitProperties.computeStreamPartitionCount = this->partitionCount;
    blitProperties.highPriority = isHighPriorityImmediateCmdList();
    if (dstPtrAlloc) {
        commandContainer.addToResidencyContainer(dstPtrAlloc);
    }
    if (srcPtrAlloc) {
        commandContainer.addToResidencyContainer(srcPtrAlloc);
    }
    commandContainer.addToResidencyContainer(clearColorAllocation);

    size_t nBlitsPerRow = NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyPerRow(blitProperties.copySize, device->getNEODevice()->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    bool useAdditionalTimestamp = nBlitsPerRow > 1;
    if (useAdditionalBlitProperties) {
        setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
    }

    NEO::BlitPropertiesContainer blitPropertiesContainer{blitProperties};
    auto blitResult = NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(blitProperties, *commandContainer.getCommandStream(), *this->dummyBlitWa.rootDeviceEnvironment);
    if (useAdditionalBlitProperties && this->isInOrderExecutionEnabled()) {
        auto inOrderCounterValue = this->inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();
        addCmdForPatching(nullptr, blitResult.lastBlitCommand, nullptr, inOrderCounterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt);
    }
    dummyBlitWa.isWaRequired = true;
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlitRegion(AlignedAllocationData *srcAllocationData,
                                                                             AlignedAllocationData *dstAllocationData,
                                                                             ze_copy_region_t srcRegion,
                                                                             ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                                                             size_t srcRowPitch, size_t srcSlicePitch,
                                                                             size_t dstRowPitch, size_t dstSlicePitch,
                                                                             const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                                             Event *signalEvent,
                                                                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                                                             bool relaxedOrderingDispatch, bool dualStreamCopyOffload) {
    srcRegion.originX += getRegionOffsetForAppendMemoryCopyBlitRegion(srcAllocationData);
    dstRegion.originX += getRegionOffsetForAppendMemoryCopyBlitRegion(dstAllocationData);

    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<GfxFamily>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originX, srcSize.x, dstSize.x);
    Vec3<size_t> srcPtrOffset = {srcRegion.originX / bytesPerPixel, srcRegion.originY, srcRegion.originZ};
    Vec3<size_t> dstPtrOffset = {dstRegion.originX / bytesPerPixel, dstRegion.originY, dstRegion.originZ};
    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();

    Vec3<size_t> copySizeModified = {copySize.x / bytesPerPixel, copySize.y, copySize.z};
    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAllocationData->alloc, srcAllocationData->alloc,
                                                                          dstPtrOffset, srcPtrOffset, copySizeModified, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation);
    commandContainer.addToResidencyContainer(dstAllocationData->alloc);
    commandContainer.addToResidencyContainer(srcAllocationData->alloc);
    commandContainer.addToResidencyContainer(clearColorAllocation);

    blitProperties.computeStreamPartitionCount = this->partitionCount;
    blitProperties.highPriority = isHighPriorityImmediateCmdList();
    blitProperties.bytesPerPixel = bytesPerPixel;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, relaxedOrderingDispatch, false, true, false, dualStreamCopyOffload);
    if (ret) {
        return ret;
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    bool copyRegionPreferred = NEO::BlitCommandsHelper<GfxFamily>::isCopyRegionPreferred(copySizeModified, rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed);
    size_t nBlits = copyRegionPreferred ? NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyRegion(blitProperties.copySize, rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed) : NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForCopyPerRow(blitProperties.copySize, rootDeviceEnvironment, blitProperties.isSystemMemoryPoolUsed);
    bool useAdditionalTimestamp = nBlits > 1;
    if (useAdditionalBlitProperties) {
        setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
    } else {
        appendEventForProfiling(signalEvent, nullptr, true, false, false, true);
    }

    NEO::BlitCommandsResult blitResult{};
    if (copyRegionPreferred) {
        blitResult = NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferRegion(blitProperties, *commandContainer.getCommandStream(), rootDeviceEnvironment);
    } else {
        blitResult = NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(blitProperties, *commandContainer.getCommandStream(), rootDeviceEnvironment);
    }
    if (useAdditionalBlitProperties && this->isInOrderExecutionEnabled()) {
        auto inOrderCounterValue = this->inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();
        addCmdForPatching(nullptr, blitResult.lastBlitCommand, nullptr, inOrderCounterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt);
    }
    dummyBlitWa.isWaRequired = true;

    if (!useAdditionalBlitProperties) {
        appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, true);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                                                      NEO::GraphicsAllocation *dst,
                                                                      const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                                                      size_t srcRowPitch, size_t srcSlicePitch,
                                                                      size_t dstRowPitch, size_t dstSlicePitch,
                                                                      size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                                                      const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                                      Event *signalEvent, uint32_t numWaitEvents,
                                                                      ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    const bool dualStreamCopyOffloadOperation = isDualStreamCopyOffloadOperation(memoryCopyParams.copyOffloadAllowed);

    auto ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, memoryCopyParams.relaxedOrderingDispatch, false, true, false, dualStreamCopyOffloadOperation);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dst, src,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation);
    blitProperties.computeStreamPartitionCount = this->partitionCount;
    blitProperties.highPriority = isHighPriorityImmediateCmdList();
    blitProperties.bytesPerPixel = bytesPerPixel;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;

    commandContainer.addToResidencyContainer(dst);
    commandContainer.addToResidencyContainer(src);
    commandContainer.addToResidencyContainer(clearColorAllocation);

    bool useAdditionalTimestamp = blitProperties.copySize.z > 1;
    if (useAdditionalBlitProperties) {
        setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
    } else {
        appendEventForProfiling(signalEvent, nullptr, true, false, false, true);
    }
    blitProperties.transform1DArrayTo2DArrayIfNeeded();

    auto blitResult = NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForImageRegion(blitProperties, *commandContainer.getCommandStream(), *dummyBlitWa.rootDeviceEnvironment);
    if (useAdditionalBlitProperties && this->isInOrderExecutionEnabled()) {
        auto inOrderCounterValue = this->inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();
        addCmdForPatching(nullptr, blitResult.lastBlitCommand, nullptr, inOrderCounterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::xyBlockCopyBlt);
    }
    dummyBlitWa.isWaRequired = true;

    if (!useAdditionalBlitProperties) {
        appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, true);
        if (this->isInOrderExecutionEnabled()) {
            appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
        }
    }
    handleInOrderDependencyCounter(signalEvent, false, false);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                                                      NEO::GraphicsAllocation *srcAllocation,
                                                                      size_t size, bool flushHost) {

    size_t middleElSize = sizeof(uint32_t) * 4;
    uintptr_t rightSize = size % middleElSize;
    bool isStateless = (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) || this->isStatelessBuiltinsEnabled();
    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }

    uintptr_t dstAddress = static_cast<uintptr_t>(dstAllocation->getGpuAddress());
    uintptr_t srcAddress = static_cast<uintptr_t>(srcAllocation->getGpuAddress());
    ze_result_t ret = ZE_RESULT_ERROR_UNKNOWN;
    if (isCopyOnly(false)) {
        return appendMemoryCopyBlit(dstAddress, dstAllocation, 0u,
                                    srcAddress, srcAllocation, 0u,
                                    size, nullptr);
    } else {
        CmdListKernelLaunchParams launchParams = {};
        launchParams.isKernelSplitOperation = rightSize > 0;
        launchParams.numKernelsInSplitLaunch = 2;
        ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAddress),
                                           dstAllocation, 0,
                                           reinterpret_cast<void *>(&srcAddress),
                                           srcAllocation, 0,
                                           size - rightSize,
                                           middleElSize,
                                           Builtin::copyBufferToBufferMiddle,
                                           nullptr,
                                           isStateless,
                                           launchParams);
        launchParams.numKernelsExecutedInSplitLaunch++;
        if (ret == ZE_RESULT_SUCCESS && rightSize) {
            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAddress),
                                               dstAllocation, size - rightSize,
                                               reinterpret_cast<void *>(&srcAddress),
                                               srcAllocation, size - rightSize,
                                               rightSize, 1UL,
                                               Builtin::copyBufferToBufferSide,
                                               nullptr,
                                               isStateless,
                                               launchParams);
            launchParams.numKernelsExecutedInSplitLaunch++;
        }

        if (this->dcFlushSupport) {
            if (flushHost) {
                NEO::PipeControlArgs args;
                args.dcFlushEnable = true;
                NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
            }
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isCopyOffloadAllowed(const NEO::GraphicsAllocation &srcAllocation, const NEO::GraphicsAllocation &dstAllocation) const {
    return !(srcAllocation.isAllocatedInLocalMemoryPool() && dstAllocation.isAllocatedInLocalMemoryPool()) && isCopyOffloadEnabled();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(void *dstptr,
                                                                   const void *srcptr,
                                                                   size_t size,
                                                                   ze_event_handle_t hSignalEvent,
                                                                   uint32_t numWaitEvents,
                                                                   ze_event_handle_t *phWaitEvents,
                                                                   CmdListMemoryCopyParams &memoryCopyParams) {

    NEO::Device *neoDevice = device->getNEODevice();
    bool sharedSystemEnabled = ((neoDevice->areSharedSystemAllocationsAllowed()) && (NEO::debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.get() == 1));

    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopy",
            callId);
    }

    auto dstAllocationStruct = getAlignedAllocationData(this->device, sharedSystemEnabled, dstptr, size, false, isCopyOffloadEnabled());
    auto srcAllocationStruct = getAlignedAllocationData(this->device, sharedSystemEnabled, srcptr, size, true, isCopyOffloadEnabled());

    if ((dstAllocationStruct.alloc == nullptr || srcAllocationStruct.alloc == nullptr) && (sharedSystemEnabled == false)) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if ((dstAllocationStruct.alloc == nullptr) && (NEO::debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.get() == 1)) {
        appendMemAdvise(device, reinterpret_cast<void *>(dstAllocationStruct.alignedAllocationPtr), size, static_cast<ze_memory_advice_t>(ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION));
    }

    if ((srcAllocationStruct.alloc == nullptr) && (NEO::debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.get() == 1)) {
        appendMemAdvise(device, reinterpret_cast<void *>(srcAllocationStruct.alignedAllocationPtr), size, static_cast<ze_memory_advice_t>(ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION));
    }

    if (dstAllocationStruct.alloc == nullptr || srcAllocationStruct.alloc == nullptr) {
        memoryCopyParams.copyOffloadAllowed = true;
    } else {
        memoryCopyParams.copyOffloadAllowed = isCopyOffloadAllowed(*srcAllocationStruct.alloc, *dstAllocationStruct.alloc);
    }
    const bool isCopyOnlyEnabled = isCopyOnly(memoryCopyParams.copyOffloadAllowed);
    const bool inOrderCopyOnlySignalingAllowed = this->isInOrderExecutionEnabled() && !memoryCopyParams.forceDisableCopyOnlyInOrderSignaling && isCopyOnlyEnabled;

    const size_t middleElSize = sizeof(uint32_t) * 4;
    uint32_t kernelCounter = 0;
    uintptr_t leftSize = 0;
    uintptr_t rightSize = 0;
    uintptr_t middleSizeBytes = 0;
    bool isStateless = (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) || this->isStatelessBuiltinsEnabled();
    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }

    const bool isHeapless = this->isHeaplessModeEnabled();

    if (!isCopyOnlyEnabled) {
        uintptr_t start = reinterpret_cast<uintptr_t>(dstptr);

        const size_t middleAlignment = MemoryConstants::cacheLineSize;

        leftSize = start % middleAlignment;

        leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0;
        leftSize = std::min(leftSize, size);

        rightSize = (start + size) % middleAlignment;
        rightSize = std::min(rightSize, size - leftSize);

        middleSizeBytes = size - leftSize - rightSize;

        if (!isAligned<4>(reinterpret_cast<uintptr_t>(srcptr) + leftSize)) {
            leftSize += middleSizeBytes;
            middleSizeBytes = 0;
        }

        DEBUG_BREAK_IF(size != leftSize + middleSizeBytes + rightSize);

        kernelCounter = leftSize > 0 ? 1 : 0;
        kernelCounter += middleSizeBytes > 0 ? 1 : 0;
        kernelCounter += rightSize > 0 ? 1 : 0;
    }

    if (NEO::debugManager.flags.ForceNonWalkerSplitMemoryCopy.get() == 1) {
        leftSize = size;
        middleSizeBytes = 0;
        rightSize = 0;
        kernelCounter = 1;
    }

    bool waitForImplicitInOrderDependency = !isCopyOnlyEnabled || inOrderCopyOnlySignalingAllowed;

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, memoryCopyParams.relaxedOrderingDispatch, false,
                                         waitForImplicitInOrderDependency, false, isDualStreamCopyOffloadOperation(memoryCopyParams.copyOffloadAllowed));

    if (ret) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    bool dcFlush = false;
    Event *signalEvent = nullptr;
    CmdListKernelLaunchParams launchParams = {};

    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
        launchParams.isHostSignalScopeEvent = signalEvent->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST);
        dcFlush = getDcFlushRequired(signalEvent->isSignalScope());
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    bool isSplitOperation = kernelCounter > 1;
    launchParams.numKernelsInSplitLaunch = kernelCounter;
    launchParams.isKernelSplitOperation = isSplitOperation;
    bool singlePipeControlPacket = eventSignalPipeControl(launchParams.isKernelSplitOperation, dcFlush);

    launchParams.pipeControlSignalling = (signalEvent && singlePipeControlPacket) || getDcFlushRequired(dstAllocationStruct.needsFlush);

    if (!useAdditionalBlitProperties || !isCopyOnlyEnabled) {
        appendEventForProfilingAllWalkers(signalEvent, nullptr, nullptr, true, singlePipeControlPacket, false, isCopyOnlyEnabled);
    }

    if (isCopyOnlyEnabled) {
        if (NEO::debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
            NEO::MiFlushArgs args{this->dummyBlitWa};
            args.tlbFlush = true;
            encodeMiFlush(0, 0, args);
        }
        ret = appendMemoryCopyBlit(dstAllocationStruct.alignedAllocationPtr,
                                   dstAllocationStruct.alloc, dstAllocationStruct.offset,
                                   srcAllocationStruct.alignedAllocationPtr,
                                   srcAllocationStruct.alloc, srcAllocationStruct.offset, size, signalEvent);
    } else {
        if (NEO::debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
            NEO::PipeControlArgs args;
            args.tlbInvalidation = true;

            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
        }

        if (ret == ZE_RESULT_SUCCESS && leftSize) {

            Builtin copyKernel = BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferSide>(isStateless, isHeapless);

            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, srcAllocationStruct.offset,
                                               leftSize, 1UL,
                                               copyKernel,
                                               signalEvent,
                                               isStateless,
                                               launchParams);
            launchParams.numKernelsExecutedInSplitLaunch++;
        }

        if (ret == ZE_RESULT_SUCCESS && middleSizeBytes) {

            Builtin copyKernel = BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(isStateless, isHeapless);

            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, leftSize + dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, leftSize + srcAllocationStruct.offset,
                                               middleSizeBytes,
                                               middleElSize,
                                               copyKernel,
                                               signalEvent,
                                               isStateless,
                                               launchParams);
            launchParams.numKernelsExecutedInSplitLaunch++;
        }

        if (ret == ZE_RESULT_SUCCESS && rightSize) {

            Builtin copyKernel = BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferSide>(isStateless, isHeapless);

            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, leftSize + middleSizeBytes + dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, leftSize + middleSizeBytes + srcAllocationStruct.offset,
                                               rightSize, 1UL,
                                               copyKernel,
                                               signalEvent,
                                               isStateless,
                                               launchParams);
            launchParams.numKernelsExecutedInSplitLaunch++;
        }
    }

    appendCopyOperationFence(signalEvent, srcAllocationStruct.alloc, dstAllocationStruct.alloc, isCopyOnlyEnabled);

    if (!useAdditionalBlitProperties || !isCopyOnlyEnabled) {
        appendEventForProfilingAllWalkers(signalEvent, nullptr, nullptr, false, singlePipeControlPacket, false, isCopyOnlyEnabled);
    }

    addToMappedEventList(signalEvent);

    if (this->isInOrderExecutionEnabled()) {
        bool emitPipeControl = !isCopyOnlyEnabled && launchParams.pipeControlSignalling;

        if ((!useAdditionalBlitProperties || !isCopyOnlyEnabled) &&
            (launchParams.isKernelSplitOperation || inOrderCopyOnlySignalingAllowed || emitPipeControl)) {
            dispatchInOrderPostOperationBarrier(signalEvent, dcFlush, isCopyOnlyEnabled);
            appendSignalInOrderDependencyCounter(signalEvent, memoryCopyParams.copyOffloadAllowed, false, false);
        }

        if (!isCopyOnlyEnabled || inOrderCopyOnlySignalingAllowed) {
            bool nonWalkerInOrderCmdChaining = !isCopyOnlyEnabled && isInOrderNonWalkerSignalingRequired(signalEvent) && !emitPipeControl;
            handleInOrderDependencyCounter(signalEvent, nonWalkerInOrderCmdChaining, isCopyOnlyEnabled);
        }
    } else {
        handleInOrderDependencyCounter(signalEvent, false, isCopyOnlyEnabled);
    }
    appendSynchronizedDispatchCleanupSection();

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopy",
            callId);
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(void *dstPtr,
                                                                         const ze_copy_region_t *dstRegion,
                                                                         uint32_t dstPitch,
                                                                         uint32_t dstSlicePitch,
                                                                         const void *srcPtr,
                                                                         const ze_copy_region_t *srcRegion,
                                                                         uint32_t srcPitch,
                                                                         uint32_t srcSlicePitch,
                                                                         ze_event_handle_t hSignalEvent,
                                                                         uint32_t numWaitEvents,
                                                                         ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopyRegion",
            callId);
    }

    size_t dstSize = this->getTotalSizeForCopyRegion(dstRegion, dstPitch, dstSlicePitch);
    size_t srcSize = this->getTotalSizeForCopyRegion(srcRegion, srcPitch, srcSlicePitch);

    auto dstAllocationStruct = getAlignedAllocationData(this->device, false, dstPtr, dstSize, false, isCopyOffloadEnabled());
    auto srcAllocationStruct = getAlignedAllocationData(this->device, false, srcPtr, srcSize, true, isCopyOffloadEnabled());

    UNRECOVERABLE_IF(srcSlicePitch && srcPitch == 0);
    Vec3<size_t> srcSize3 = {srcPitch ? srcPitch : srcRegion->width + srcRegion->originX,
                             srcSlicePitch ? srcSlicePitch / srcPitch : srcRegion->height + srcRegion->originY,
                             srcRegion->depth + srcRegion->originZ};
    UNRECOVERABLE_IF(dstSlicePitch && dstPitch == 0);
    Vec3<size_t> dstSize3 = {dstPitch ? dstPitch : dstRegion->width + dstRegion->originX,
                             dstSlicePitch ? dstSlicePitch / dstPitch : dstRegion->height + dstRegion->originY,
                             dstRegion->depth + dstRegion->originZ};

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (dstAllocationStruct.alloc == nullptr || srcAllocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    memoryCopyParams.copyOffloadAllowed = isCopyOffloadAllowed(*srcAllocationStruct.alloc, *dstAllocationStruct.alloc);
    const bool isCopyOnlyEnabled = isCopyOnly(memoryCopyParams.copyOffloadAllowed);
    const bool inOrderCopyOnlySignalingAllowed = this->isInOrderExecutionEnabled() && !memoryCopyParams.forceDisableCopyOnlyInOrderSignaling && isCopyOnlyEnabled;

    ze_result_t result = ZE_RESULT_SUCCESS;
    if (isCopyOnlyEnabled) {
        result = appendMemoryCopyBlitRegion(&srcAllocationStruct, &dstAllocationStruct, *srcRegion, *dstRegion,
                                            {srcRegion->width, srcRegion->height, srcRegion->depth},
                                            srcPitch, srcSlicePitch, dstPitch, dstSlicePitch, srcSize3, dstSize3,
                                            signalEvent, numWaitEvents, phWaitEvents, memoryCopyParams.relaxedOrderingDispatch, isDualStreamCopyOffloadOperation(memoryCopyParams.copyOffloadAllowed));
    } else if ((srcRegion->depth > 1) || (srcRegion->originZ != 0) || (dstRegion->originZ != 0)) {
        result = this->appendMemoryCopyKernel3d(&dstAllocationStruct, &srcAllocationStruct, Builtin::copyBufferRectBytes3d,
                                                dstRegion, dstPitch, dstSlicePitch, dstAllocationStruct.offset,
                                                srcRegion, srcPitch, srcSlicePitch, srcAllocationStruct.offset,
                                                signalEvent, numWaitEvents, phWaitEvents, memoryCopyParams.relaxedOrderingDispatch);
    } else {
        result = this->appendMemoryCopyKernel2d(&dstAllocationStruct, &srcAllocationStruct, Builtin::copyBufferRectBytes2d,
                                                dstRegion, dstPitch, dstAllocationStruct.offset,
                                                srcRegion, srcPitch, srcAllocationStruct.offset,
                                                signalEvent, numWaitEvents, phWaitEvents, memoryCopyParams.relaxedOrderingDispatch);
    }

    if (result) {
        return result;
    }

    appendCopyOperationFence(signalEvent, srcAllocationStruct.alloc, dstAllocationStruct.alloc, isCopyOnlyEnabled);

    addToMappedEventList(signalEvent);

    if (this->isInOrderExecutionEnabled()) {
        if (inOrderCopyOnlySignalingAllowed) {
            if (!useAdditionalBlitProperties) {
                appendSignalInOrderDependencyCounter(signalEvent, memoryCopyParams.copyOffloadAllowed, false, false);
            }
            handleInOrderDependencyCounter(signalEvent, false, isCopyOnlyEnabled);
        }
    } else {
        handleInOrderDependencyCounter(signalEvent, false, isCopyOnlyEnabled);
    }

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopyRegion",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation,
                                                                           AlignedAllocationData *srcAlignedAllocation,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           uint32_t dstSlicePitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           uint32_t srcSlicePitch,
                                                                           size_t srcOffset,
                                                                           Event *signalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());

    auto builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = srcRegion->depth;

    ze_result_t ret = builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                                      &groupSizeX, &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY || srcRegion->depth % groupSizeZ) {
        CREATE_DEBUG_STRING(str, "Invalid group size {%d, %d, %d} specified\n", groupSizeX, groupSizeY, groupSizeZ);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Invalid group size {%d, %d, %d} specified\n",
                           groupSizeX, groupSizeY, groupSizeZ);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchKernelArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY,
                                        srcRegion->depth / groupSizeZ};

    uint32_t srcOrigin[3] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY), (srcRegion->originZ)};
    uint32_t dstOrigin[3] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY), (dstRegion->originZ)};
    uint32_t srcPitches[2] = {(srcPitch), (srcSlicePitch)};
    uint32_t dstPitches[2] = {(dstPitch), (dstSlicePitch)};

    builtinKernel->setArgBufferWithAlloc(0, srcAlignedAllocation->alignedAllocationPtr, srcAlignedAllocation->alloc, nullptr);
    builtinKernel->setArgBufferWithAlloc(1, dstAlignedAllocation->alignedAllocationPtr, dstAlignedAllocation->alloc, nullptr);
    builtinKernel->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinKernel->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinKernel->setArgumentValue(4, sizeof(srcPitches), &srcPitches);
    builtinKernel->setArgumentValue(5, sizeof(dstPitches), &dstPitches);

    auto dstAllocationType = dstAlignedAllocation->alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::bufferHostMemory) ||
        (dstAllocationType == NEO::AllocationType::externalHostPtr);

    if constexpr (checkIfAllocationImportedRequired()) {
        launchParams.isDestinationAllocationImported = this->isAllocationImported(dstAlignedAllocation->alloc, device->getDriverHandle()->getSvmAllocsManager());
    }
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), dispatchKernelArgs, signalEvent, numWaitEvents,
                                                                    phWaitEvents, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation,
                                                                           AlignedAllocationData *srcAlignedAllocation,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           size_t srcOffset,
                                                                           Event *signalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());

    auto builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = 1u;

    ze_result_t ret = builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX,
                                                      &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY) {
        CREATE_DEBUG_STRING(str, "Invalid group size {%d, %d} specified\n", groupSizeX, groupSizeY);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Invalid group size {%d, %d}\n",
                           groupSizeX, groupSizeY);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchKernelArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY, 1u};

    uint32_t srcOrigin[2] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY)};
    uint32_t dstOrigin[2] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY)};

    builtinKernel->setArgBufferWithAlloc(0, srcAlignedAllocation->alignedAllocationPtr, srcAlignedAllocation->alloc, nullptr);
    builtinKernel->setArgBufferWithAlloc(1, dstAlignedAllocation->alignedAllocationPtr, dstAlignedAllocation->alloc, nullptr);
    builtinKernel->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinKernel->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinKernel->setArgumentValue(4, sizeof(srcPitch), &srcPitch);
    builtinKernel->setArgumentValue(5, sizeof(dstPitch), &dstPitch);

    auto dstAllocationType = dstAlignedAllocation->alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::bufferHostMemory) ||
        (dstAllocationType == NEO::AllocationType::externalHostPtr);

    if constexpr (CommandListCoreFamily<gfxCoreFamily>::checkIfAllocationImportedRequired()) {
        launchParams.isDestinationAllocationImported = this->isAllocationImported(dstAlignedAllocation->alloc, device->getDriverHandle()->getSvmAllocsManager());
    }
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(),
                                                                    dispatchKernelArgs, signalEvent,
                                                                    numWaitEvents,
                                                                    phWaitEvents,
                                                                    launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryPrefetch(const void *ptr, size_t size) {
    auto svmAllocMgr = device->getDriverHandle()->getSvmAllocsManager();
    auto allocData = svmAllocMgr->getSVMAlloc(ptr);

    if (!allocData) {
        if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
            this->performMemoryPrefetch = true;
            auto prefetchManager = device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
            if (prefetchManager) {
                prefetchManager->insertAllocation(this->prefetchContext, *device->getDriverHandle()->getSvmAllocsManager(), *device->getNEODevice(), ptr, size);
            }
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    if (NEO::debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get() == true) {
        this->performMemoryPrefetch = true;
        auto prefetchManager = device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        if (prefetchManager) {
            prefetchManager->insertAllocation(this->prefetchContext, *device->getDriverHandle()->getSvmAllocsManager(), *device->getNEODevice(), ptr, size);
        }
    }

    return ZE_RESULT_SUCCESS;
}

static inline void builtinSetArgFill(Kernel *builtinKernel, uint32_t argIndex, uintptr_t argPtr, NEO::GraphicsAllocation *allocation) {
    if (allocation) {
        builtinKernel->setArgBufferWithAlloc(argIndex, argPtr, allocation, nullptr);
    } else {
        builtinKernel->setArgumentValue(argIndex, sizeof(argPtr), &argPtr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendUnalignedFillKernel(bool isStateless, uint32_t unalignedSize, const AlignedAllocationData &dstAllocation, const void *pattern, Event *signalEvent, CmdListKernelLaunchParams &launchParams) {

    const bool isHeapless = this->isHeaplessModeEnabled();
    auto builtin = BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(isStateless, isHeapless);

    Kernel *builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);
    uint32_t groupSizeY = 1, groupSizeZ = 1;
    uint32_t groupSizeX = static_cast<uint32_t>(unalignedSize);
    builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
    builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    ze_group_count_t dispatchKernelRemainderArgs{static_cast<uint32_t>(unalignedSize / groupSizeX), 1u, 1u};
    uint32_t value = *(reinterpret_cast<const unsigned char *>(pattern));
    builtinSetArgFill(builtinKernel, 0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
    builtinKernel->setArgumentValue(1, sizeof(dstAllocation.offset), &dstAllocation.offset);
    builtinKernel->setArgumentValue(2, sizeof(value), &value);

    auto res = appendLaunchKernelSplit(builtinKernel, dispatchKernelRemainderArgs, signalEvent, launchParams);
    if (res) {
        return res;
    }
    return ZE_RESULT_SUCCESS;
}

inline bool canUseImmediateFill(size_t size, size_t patternSize, size_t offset, size_t maxWgSize) {
    return patternSize == 1 || (patternSize <= 4 &&
                                isAligned<sizeof(uint32_t)>(offset) &&
                                isAligned<sizeof(uint32_t) * 4>(size) &&
                                (size <= maxWgSize || isAligned(size / (sizeof(uint32_t) * 4), maxWgSize)));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(void *ptr,
                                                                   const void *pattern,
                                                                   size_t patternSize,
                                                                   size_t size,
                                                                   ze_event_handle_t hSignalEvent,
                                                                   uint32_t numWaitEvents,
                                                                   ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    bool isStateless = (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) || this->isStatelessBuiltinsEnabled();
    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }

    const bool isHeapless = this->isHeaplessModeEnabled();
    memoryCopyParams.copyOffloadAllowed = isCopyOffloadEnabled();

    NEO::Device *neoDevice = device->getNEODevice();
    bool sharedSystemEnabled = ((neoDevice->areSharedSystemAllocationsAllowed()) && (NEO::debugManager.flags.TreatNonUsmForTransfersAsSharedSystem.get() == 1));
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryFill",
            callId);
    }

    CmdListKernelLaunchParams launchParams = {};

    Event *signalEvent = nullptr;
    bool dcFlush = false;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
        launchParams.isHostSignalScopeEvent = signalEvent->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST);
        dcFlush = getDcFlushRequired(signalEvent->isSignalScope());
    }

    if (isCopyOnly(memoryCopyParams.copyOffloadAllowed)) {
        auto status = appendBlitFill(ptr, pattern, patternSize, size, signalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
        addToMappedEventList(signalEvent);
        return status;
    }

    ze_result_t res = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, memoryCopyParams.relaxedOrderingDispatch, false, true, false, false);
    if (res) {
        return res;
    }

    appendSynchronizedDispatchInitializationSection();

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    bool hostPointerNeedsFlush = false;

    NEO::SvmAllocationData *allocData = nullptr;
    bool dstAllocFound = device->getDriverHandle()->findAllocationDataForRange(ptr, size, allocData);
    if (dstAllocFound) {
        if (allocData->memoryType == InternalMemoryType::hostUnifiedMemory ||
            allocData->memoryType == InternalMemoryType::sharedUnifiedMemory) {
            hostPointerNeedsFlush = true;
        }

    } else {
        if ((sharedSystemEnabled == false) && (neoDevice->areSharedSystemAllocationsAllowed() == false) && (device->getDriverHandle()->getHostPointerBaseAddress(ptr, nullptr) != ZE_RESULT_SUCCESS)) {
            // first two conditions, above are default, and each may be turned true only with debug variables
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        hostPointerNeedsFlush = true;
    }

    auto dstAllocation = this->getAlignedAllocationData(this->device, sharedSystemEnabled, ptr, size, false, false);
    if ((dstAllocation.alloc == nullptr) && (sharedSystemEnabled == false)) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if ((dstAllocation.alloc == nullptr) && (NEO::debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.get() == 1)) {
        appendMemAdvise(device, reinterpret_cast<void *>(dstAllocation.alignedAllocationPtr), size, static_cast<ze_memory_advice_t>(ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION));
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    bool useImmediateFill = canUseImmediateFill(size, patternSize, dstAllocation.offset, this->device->getDeviceInfo().maxWorkGroupSize);
    auto builtin = useImmediateFill
                       ? BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediate>(isStateless, isHeapless)
                       : BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferMiddle>(isStateless, isHeapless);

    Kernel *builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory = hostPointerNeedsFlush;
    if (dstAllocation.alloc) {
        if constexpr (checkIfAllocationImportedRequired()) {
            launchParams.isDestinationAllocationImported = this->isAllocationImported(dstAllocation.alloc, device->getDriverHandle()->getSvmAllocsManager());
        }
    }
    CmdListFillKernelArguments fillArguments = {};
    setupFillKernelArguments(dstAllocation.offset, patternSize, size, fillArguments, builtinKernel);

    launchParams.isKernelSplitOperation = (fillArguments.leftRemainingBytes > 0 || fillArguments.rightRemainingBytes > 0);
    bool singlePipeControlPacket = eventSignalPipeControl(launchParams.isKernelSplitOperation, dcFlush);
    launchParams.pipeControlSignalling = (signalEvent && singlePipeControlPacket) || getDcFlushRequired(dstAllocation.needsFlush);

    appendEventForProfilingAllWalkers(signalEvent, nullptr, nullptr, true, singlePipeControlPacket, false, isCopyOnly(false));

    if (fillArguments.leftRemainingBytes > 0) {
        launchParams.numKernelsInSplitLaunch++;
    }
    if (fillArguments.rightRemainingBytes > 0) {
        launchParams.numKernelsInSplitLaunch++;
    }

    if (useImmediateFill) {
        launchParams.numKernelsInSplitLaunch++;
        if (fillArguments.leftRemainingBytes > 0) {
            DEBUG_BREAK_IF(useImmediateFill && patternSize > 1u);
            res = appendUnalignedFillKernel(isStateless, fillArguments.leftRemainingBytes, dstAllocation, pattern, signalEvent, launchParams);
            if (res) {
                return res;
            }
            launchParams.numKernelsExecutedInSplitLaunch++;
        }

        ze_result_t ret = builtinKernel->setGroupSize(static_cast<uint32_t>(fillArguments.mainGroupSize), 1u, 1u);
        if (ret != ZE_RESULT_SUCCESS) {
            DEBUG_BREAK_IF(true);
            return ret;
        }

        ze_group_count_t dispatchKernelArgs{static_cast<uint32_t>(fillArguments.groups), 1u, 1u};

        uint32_t value = 0u;

        switch (patternSize) {
        case 1:
            memset(&value, *reinterpret_cast<const unsigned char *>(pattern), 4);
            break;
        case 2:
            memcpy(&value, pattern, 2);
            value <<= 16;
            memcpy(&value, pattern, 2);
            break;
        case 4:
            memcpy(&value, pattern, 4);
            break;
        default:
            UNRECOVERABLE_IF(true);
        }

        builtinSetArgFill(builtinKernel, 0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
        builtinKernel->setArgumentValue(1, sizeof(fillArguments.mainOffset), &fillArguments.mainOffset);
        builtinKernel->setArgumentValue(2, sizeof(value), &value);

        res = appendLaunchKernelSplit(builtinKernel, dispatchKernelArgs, signalEvent, launchParams);
        if (res) {
            return res;
        }
        launchParams.numKernelsExecutedInSplitLaunch++;

        if (fillArguments.rightRemainingBytes > 0) {
            DEBUG_BREAK_IF(useImmediateFill && patternSize > 1u);
            dstAllocation.offset = fillArguments.rightOffset;
            res = appendUnalignedFillKernel(isStateless, fillArguments.rightRemainingBytes, dstAllocation, pattern, signalEvent, launchParams);
            if (res) {
                return res;
            }
            launchParams.numKernelsExecutedInSplitLaunch++;
        }
    } else {
        builtinKernel->setGroupSize(static_cast<uint32_t>(fillArguments.mainGroupSize), 1, 1);

        NEO::GraphicsAllocation *patternGfxAlloc = nullptr;
        void *patternGfxAllocPtr = nullptr;
        size_t patternAllocationSize = alignUp(patternSize, MemoryConstants::cacheLineSize);

        if (patternAllocationSize > MemoryConstants::cacheLineSize) {
            patternGfxAlloc = device->obtainReusableAllocation(patternAllocationSize, NEO::AllocationType::fillPattern);
            if (patternGfxAlloc == nullptr) {
                NEO::AllocationProperties allocationProperties{device->getNEODevice()->getRootDeviceIndex(),
                                                               patternAllocationSize,
                                                               NEO::AllocationType::fillPattern,
                                                               device->getNEODevice()->getDeviceBitfield()};
                allocationProperties.alignment = MemoryConstants::pageSize;
                patternGfxAlloc = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocationProperties);
            }
            patternGfxAllocPtr = patternGfxAlloc->getUnderlyingBuffer();
            patternAllocations.push_back(patternGfxAlloc);
        } else {
            auto patternTag = device->getFillPatternAllocator()->getTag();
            patternGfxAllocPtr = patternTag->getCpuBase();
            patternGfxAlloc = patternTag->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex());
            this->patternTags.push_back(patternTag);
            commandContainer.addToResidencyContainer(patternGfxAlloc);
        }

        uint64_t patternAllocPtr = reinterpret_cast<uintptr_t>(patternGfxAllocPtr);
        uint64_t patternAllocOffset = 0;
        uint64_t patternSizeToCopy = patternSize;
        do {
            memcpy_s(reinterpret_cast<void *>(patternAllocPtr + patternAllocOffset),
                     patternSizeToCopy, pattern, patternSizeToCopy);

            if ((patternAllocOffset + patternSizeToCopy) > patternAllocationSize) {
                patternSizeToCopy = patternAllocationSize - patternAllocOffset;
            }

            patternAllocOffset += patternSizeToCopy;
        } while (patternAllocOffset < patternAllocationSize);
        if (fillArguments.leftRemainingBytes == 0) {
            builtinSetArgFill(builtinKernel, 0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
            builtinKernel->setArgumentValue(1, sizeof(dstAllocation.offset), &dstAllocation.offset);
            builtinKernel->setArgBufferWithAlloc(2, reinterpret_cast<uintptr_t>(patternGfxAllocPtr), patternGfxAlloc, nullptr);
            builtinKernel->setArgumentValue(3, sizeof(fillArguments.patternSizeInEls), &fillArguments.patternSizeInEls);

            ze_group_count_t dispatchKernelArgs{static_cast<uint32_t>(fillArguments.groups), 1u, 1u};
            launchParams.numKernelsInSplitLaunch++;
            res = appendLaunchKernelSplit(builtinKernel, dispatchKernelArgs, signalEvent, launchParams);
            if (res) {
                return res;
            }
            launchParams.numKernelsExecutedInSplitLaunch++;
        } else {
            uint32_t dstOffsetRemainder = static_cast<uint32_t>(dstAllocation.offset);

            auto builtin = BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferRightLeftover>(isStateless, isHeapless);
            Kernel *builtinKernelRemainder = device->getBuiltinFunctionsLib()->getFunction(builtin);

            builtinKernelRemainder->setGroupSize(static_cast<uint32_t>(fillArguments.mainGroupSize), 1, 1);
            ze_group_count_t dispatchKernelArgs{static_cast<uint32_t>(fillArguments.groups), 1u, 1u};

            builtinSetArgFill(builtinKernelRemainder, 0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
            builtinKernelRemainder->setArgumentValue(1,
                                                     sizeof(dstOffsetRemainder),
                                                     &dstOffsetRemainder);
            builtinKernelRemainder->setArgBufferWithAlloc(2,
                                                          reinterpret_cast<uintptr_t>(patternGfxAllocPtr),
                                                          patternGfxAlloc, nullptr);
            builtinKernelRemainder->setArgumentValue(3, sizeof(patternAllocationSize), &patternAllocationSize);

            res = appendLaunchKernelSplit(builtinKernelRemainder, dispatchKernelArgs, signalEvent, launchParams);
            if (res) {
                return res;
            }
            launchParams.numKernelsExecutedInSplitLaunch++;
        }

        if (fillArguments.rightRemainingBytes > 0) {
            uint32_t dstOffsetRemainder = static_cast<uint32_t>(fillArguments.rightOffset);
            uint64_t patternOffsetRemainder = fillArguments.patternOffsetRemainder;

            auto builtin = BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferRightLeftover>(isStateless, isHeapless);
            Kernel *builtinKernelRemainder = device->getBuiltinFunctionsLib()->getFunction(builtin);

            builtinKernelRemainder->setGroupSize(fillArguments.rightRemainingBytes, 1u, 1u);
            ze_group_count_t dispatchKernelArgs{1u, 1u, 1u};

            builtinSetArgFill(builtinKernelRemainder, 0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
            builtinKernelRemainder->setArgumentValue(1,
                                                     sizeof(dstOffsetRemainder),
                                                     &dstOffsetRemainder);
            builtinKernelRemainder->setArgBufferWithAlloc(2,
                                                          reinterpret_cast<uintptr_t>(patternGfxAllocPtr) + patternOffsetRemainder,
                                                          patternGfxAlloc, nullptr);
            builtinKernelRemainder->setArgumentValue(3, sizeof(patternAllocationSize), &patternAllocationSize);

            res = appendLaunchKernelSplit(builtinKernelRemainder, dispatchKernelArgs, signalEvent, launchParams);
            if (res) {
                return res;
            }
            launchParams.numKernelsExecutedInSplitLaunch++;
        }
    }

    addToMappedEventList(signalEvent);
    appendEventForProfilingAllWalkers(signalEvent, nullptr, nullptr, false, singlePipeControlPacket, false, isCopyOnly(false));

    bool nonWalkerInOrderCmdChaining = false;
    if (this->isInOrderExecutionEnabled()) {
        if (launchParams.isKernelSplitOperation || launchParams.pipeControlSignalling) {
            dispatchInOrderPostOperationBarrier(signalEvent, dcFlush, isCopyOnly(false));
            appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
        } else {
            nonWalkerInOrderCmdChaining = isInOrderNonWalkerSignalingRequired(signalEvent);
        }
    }
    handleInOrderDependencyCounter(signalEvent, nonWalkerInOrderCmdChaining, false);

    appendSynchronizedDispatchCleanupSection();

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryFill",
            callId);
    }

    return res;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBlitFill(void *ptr, const void *pattern, size_t patternSize, size_t size, Event *signalEvent, uint32_t numWaitEvents,
                                                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {

    NEO::Device *neoDevice = device->getNEODevice();
    bool sharedSystemEnabled = neoDevice->areSharedSystemAllocationsAllowed();

    if (this->maxFillPaternSizeForCopyEngine < patternSize) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    } else {
        const bool dualStreamCopyOffloadOperation = isDualStreamCopyOffloadOperation(memoryCopyParams.copyOffloadAllowed);
        const bool isCopyOnlySignaling = isCopyOnly(dualStreamCopyOffloadOperation) && !useAdditionalBlitProperties;

        ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, memoryCopyParams.relaxedOrderingDispatch, false, true, false, dualStreamCopyOffloadOperation);
        if (ret) {
            return ret;
        }

        if (!handleCounterBasedEventOperations(signalEvent, false)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto neoDevice = device->getNEODevice();
        if (isCopyOnlySignaling) {
            appendEventForProfiling(signalEvent, nullptr, true, false, false, true);
        }
        NEO::GraphicsAllocation *gpuAllocation = device->getDriverHandle()->getDriverSystemMemoryAllocation(ptr,
                                                                                                            size,
                                                                                                            neoDevice->getRootDeviceIndex(),
                                                                                                            nullptr);

        DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
        auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
        if (driverHandle->isRemoteResourceNeeded(ptr, gpuAllocation, allocData, device)) {
            if (allocData) {
                uint64_t pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
                gpuAllocation = driverHandle->getPeerAllocation(device, allocData, reinterpret_cast<void *>(pbase), nullptr, nullptr);
            }
            if ((gpuAllocation == nullptr) && (sharedSystemEnabled == false)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        uint32_t patternToCommand[4] = {};
        memcpy_s(&patternToCommand, sizeof(patternToCommand), pattern, patternSize);
        NEO::BlitProperties blitProperties;
        bool useAdditionalTimestamp = false;
        if (gpuAllocation) {
            auto offset = getAllocationOffsetForAppendBlitFill(ptr, *gpuAllocation);

            commandContainer.addToResidencyContainer(gpuAllocation);

            blitProperties = NEO::BlitProperties::constructPropertiesForMemoryFill(gpuAllocation, size, patternToCommand, patternSize, offset);
            size_t nBlits = NEO::BlitCommandsHelper<GfxFamily>::getNumberOfBlitsForColorFill(blitProperties.copySize, patternSize, device->getNEODevice()->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
            useAdditionalTimestamp = nBlits > 1;
        } else if (sharedSystemEnabled == true) {
            if (NEO::debugManager.flags.EmitMemAdvisePriorToCopyForNonUsm.get() == 1) {
                appendMemAdvise(device, ptr, size, static_cast<ze_memory_advice_t>(ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION));
            }
            blitProperties = NEO::BlitProperties::constructPropertiesForSystemMemoryFill(reinterpret_cast<uint64_t>(ptr), size, patternToCommand, patternSize, 0ul);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        if (useAdditionalBlitProperties) {
            setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
        }
        blitProperties.computeStreamPartitionCount = this->partitionCount;
        blitProperties.highPriority = isHighPriorityImmediateCmdList();

        auto blitResult = NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(blitProperties, *commandContainer.getCommandStream(), neoDevice->getRootDeviceEnvironmentRef());
        if (useAdditionalBlitProperties && this->isInOrderExecutionEnabled()) {
            using PatchCmdType = NEO::InOrderPatchCommandHelpers::PatchCmdType;
            PatchCmdType patchCmdType = (blitProperties.fillPatternSize == 1) ? PatchCmdType::memSet : PatchCmdType::xyColorBlt;
            auto inOrderCounterValue = this->inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();
            addCmdForPatching(nullptr, blitResult.lastBlitCommand, nullptr, inOrderCounterValue, patchCmdType);
        }
        dummyBlitWa.isWaRequired = true;
        if (isCopyOnlySignaling) {
            appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, true);
        }

        if (isInOrderExecutionEnabled() && isCopyOnlySignaling) {
            appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
        }
        handleInOrderDependencyCounter(signalEvent, false, memoryCopyParams.copyOffloadAllowed);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSignalEventPostWalker(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outTimeStampSyncCmds, bool skipBarrierForEndProfiling, bool skipAddingEventToResidency, bool copyOperation) {
    if (event == nullptr || !event->getAllocation(this->device)) {
        return;
    }
    if (event->isEventTimestampFlagSet()) {
        appendEventForProfiling(event, outTimeStampSyncCmds, false, skipBarrierForEndProfiling, skipAddingEventToResidency, copyOperation);
    } else {
        event->resetKernelCountAndPacketUsedCount();
        if (!skipAddingEventToResidency) {
            commandContainer.addToResidencyContainer(event->getAllocation(this->device));
        }

        event->setPacketsInUse(copyOperation ? 1 : this->partitionCount);
        dispatchEventPostSyncOperation(event, syncCmdBuffer, nullptr, Event::STATE_SIGNALED, false, false, !copyOperation, false, copyOperation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfilingCopyCommand(Event *event, bool beforeWalker) {
    if (!event->isEventTimestampFlagSet()) {
        return;
    }
    commandContainer.addToResidencyContainer(event->getAllocation(this->device));
    if (beforeWalker) {
        event->resetKernelCountAndPacketUsedCount();
    } else {
        NEO::MiFlushArgs args{this->dummyBlitWa};
        encodeMiFlush(0, 0, args);
        dispatchEventPostSyncOperation(event, nullptr, nullptr, Event::STATE_SIGNALED, true, false, false, false, true);
    }
    appendWriteKernelTimestamp(event, nullptr, beforeWalker, false, false, true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline uint64_t CommandListCoreFamily<gfxCoreFamily>::getInputBufferSize(NEO::ImageType imageType,
                                                                         uint32_t bufferRowPitch,
                                                                         uint32_t bufferSlicePitch,
                                                                         const ze_image_region_t *region) {
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    switch (imageType) {
    default: {
        CREATE_DEBUG_STRING(str, "invalid imageType: %d\n", static_cast<int>(imageType));
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "invalid imageType: %d\n", imageType);
        UNRECOVERABLE_IF(true);
        return 0;
    }
    case NEO::ImageType::image1D:
        return bufferRowPitch;
    case NEO::ImageType::image1DArray:
    case NEO::ImageType::image2D:
        return static_cast<uint64_t>(bufferRowPitch) * region->height;
    case NEO::ImageType::image2DArray:
    case NEO::ImageType::image3D:
        return static_cast<uint64_t>(bufferSlicePitch) * region->depth;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline AlignedAllocationData CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocationData(Device *device, bool sharedSystemEnabled, const void *buffer, uint64_t bufferSize, bool hostCopyAllowed, bool copyOffload) {
    NEO::SvmAllocationData *allocData = nullptr;
    void *ptr = const_cast<void *>(buffer);
    bool srcAllocFound = device->getDriverHandle()->findAllocationDataForRange(ptr,
                                                                               bufferSize, allocData);
    NEO::GraphicsAllocation *alloc = nullptr;

    uintptr_t sourcePtr = reinterpret_cast<uintptr_t>(ptr);
    size_t offset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(sourcePtr, offset);
    uintptr_t alignedPtr = 0u;
    bool hostPointerNeedsFlush = false;

    if (srcAllocFound == false) {
        alloc = device->getDriverHandle()->findHostPointerAllocation(ptr, static_cast<size_t>(bufferSize), device->getRootDeviceIndex());
        if (alloc != nullptr) {
            alignedPtr = static_cast<size_t>(alignDown(alloc->getGpuAddress(), NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment()));
            // get offset from GPUVA of allocation to align down GPU address
            offset = static_cast<size_t>(alloc->getGpuAddress()) - alignedPtr;
            // get offset from base of allocation to arg address
            offset += reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(alloc->getUnderlyingBuffer());
        } else {
            if (sharedSystemEnabled) {
                return {reinterpret_cast<uintptr_t>(ptr), 0, nullptr, true};
            } else {
                alloc = getHostPtrAlloc(buffer, bufferSize, hostCopyAllowed, copyOffload);
                if (alloc == nullptr) {
                    return {0u, 0, nullptr, false};
                }
                alignedPtr = static_cast<uintptr_t>(alignDown(alloc->getGpuAddress(), NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment()));
                if (alloc->getAllocationType() == NEO::AllocationType::externalHostPtr) {
                    auto hostAllocCpuPtr = reinterpret_cast<uintptr_t>(alloc->getUnderlyingBuffer());
                    hostAllocCpuPtr = alignDown(hostAllocCpuPtr, NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment());
                    auto allignedPtrOffset = sourcePtr - hostAllocCpuPtr;
                    alignedPtr = ptrOffset(alignedPtr, allignedPtrOffset);
                }
            }
        }

        hostPointerNeedsFlush = true;
    } else {
        alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
        DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(deviceImp->getDriverHandle());
        if (driverHandle->isRemoteResourceNeeded(const_cast<void *>(buffer), alloc, allocData, device)) {
            uint64_t pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
            uint64_t offset = sourcePtr - pbase;

            alloc = driverHandle->getPeerAllocation(device, allocData, reinterpret_cast<void *>(pbase), &alignedPtr, nullptr);
            alignedPtr += offset;

            if (allocData->memoryType == InternalMemoryType::sharedUnifiedMemory) {
                commandContainer.addToResidencyContainer(allocData->gpuAllocations.getDefaultGraphicsAllocation());
            }
        } else {
            alignedPtr = sourcePtr;
        }

        if (allocData->memoryType == InternalMemoryType::hostUnifiedMemory ||
            allocData->memoryType == InternalMemoryType::sharedUnifiedMemory) {
            hostPointerNeedsFlush = true;
        }
        if (allocData->virtualReservationData) {
            for (const auto &mappedAllocationData : allocData->virtualReservationData->mappedAllocations) {
                // Add additional allocations to the residency container if the virtual reservation spans multiple allocations.
                if (buffer != mappedAllocationData.second->ptr) {
                    commandContainer.addToResidencyContainer(mappedAllocationData.second->mappedAllocation.allocation);
                }
            }
        }
    }

    return {alignedPtr, offset, alloc, hostPointerNeedsFlush};
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::getAllocationOffsetForAppendBlitFill(void *ptr, NEO::GraphicsAllocation &gpuAllocation) {
    uint64_t offset;
    if (gpuAllocation.getAllocationType() == NEO::AllocationType::externalHostPtr) {
        offset = castToUint64(ptr) - castToUint64(gpuAllocation.getUnderlyingBuffer()) + gpuAllocation.getAllocationOffset();
    } else {
        offset = castToUint64(ptr) - gpuAllocation.getGpuAddress();
    }
    return offset;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline uint32_t CommandListCoreFamily<gfxCoreFamily>::getRegionOffsetForAppendMemoryCopyBlitRegion(AlignedAllocationData *allocationData) {
    uint64_t ptr = allocationData->alignedAllocationPtr + allocationData->offset;
    uint64_t allocPtr = allocationData->alloc->getGpuAddress();
    return static_cast<uint32_t>(ptr - allocPtr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::handleInOrderImplicitDependencies(bool relaxedOrderingAllowed, bool dualStreamCopyOffloadOperation) {
    if (hasInOrderDependencies()) {
        if (inOrderExecInfo->isCounterAlreadyDone(inOrderExecInfo->getCounterValue())) {
            return false;
        }

        if (relaxedOrderingAllowed) {
            NEO::RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(*commandContainer.getCommandStream(), isCopyOnly(dualStreamCopyOffloadOperation));
        }

        CommandListCoreFamily<gfxCoreFamily>::appendWaitOnInOrderDependency(inOrderExecInfo, nullptr, inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset(), relaxedOrderingAllowed, true, false, false, dualStreamCopyOffloadOperation);

        return true;
    }

    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamily<gfxCoreFamily>::addEventsToCmdList(uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CommandToPatchContainer *outWaitCmds,
                                                                            bool relaxedOrderingAllowed, bool trackDependencies, bool waitForImplicitInOrderDependency, bool skipAddingWaitEventsToResidency, bool dualStreamCopyOffloadOperation) {
    bool inOrderDependenciesSent = false;

    if (this->latestOperationRequiredNonWalkerInOrderCmdsChaining && !relaxedOrderingAllowed) {
        waitForImplicitInOrderDependency = false;
    }

    if (waitForImplicitInOrderDependency) {
        auto ret = this->flushInOrderCounterSignal(dualStreamCopyOffloadOperation || relaxedOrderingAllowed);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }

        inOrderDependenciesSent = handleInOrderImplicitDependencies(relaxedOrderingAllowed, dualStreamCopyOffloadOperation);
        this->latestOperationHasOptimizedCbEvent = false;
    }

    if (relaxedOrderingAllowed && numWaitEvents > 0 && !inOrderDependenciesSent) {
        NEO::RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(*commandContainer.getCommandStream(), isCopyOnly(dualStreamCopyOffloadOperation));
    }

    if (numWaitEvents > 0) {
        if (phWaitEvents) {
            return CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numWaitEvents, phWaitEvents, outWaitCmds, relaxedOrderingAllowed, trackDependencies, false, skipAddingWaitEventsToResidency, false, dualStreamCopyOffloadOperation);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) {
    if (this->isInOrderExecutionEnabled()) {
        handleInOrderImplicitDependencies(relaxedOrderingDispatch, false);
    }

    auto event = Event::fromHandle(hEvent);
    event->resetKernelCountAndPacketUsedCount();

    if (!handleCounterBasedEventOperations(event, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    commandContainer.addToResidencyContainer(event->getAllocation(this->device));
    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendSignalEvent",
            callId);
    }

    event->setPacketsInUse(this->partitionCount);
    bool appendPipeControlWithPostSync = (!isCopyOnly(false)) && (event->isSignalScope() || event->isEventTimestampFlagSet());
    dispatchEventPostSyncOperation(event, nullptr, nullptr, Event::STATE_SIGNALED, false, false, appendPipeControlWithPostSync, false, isCopyOnly(false));

    if (!event->getAllocation(this->device) && appendPipeControlWithPostSync && getDcFlushRequired(true)) {
        NEO::PipeControlArgs pipeControlArgs;
        pipeControlArgs.dcFlushEnable = true;

        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), pipeControlArgs);
    }

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(event, false, false, false);
    }
    handleInOrderDependencyCounter(event, false, false);

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendSignalEvent",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::GraphicsAllocation *CommandListCoreFamily<gfxCoreFamily>::getDeviceCounterAllocForResidency(NEO::GraphicsAllocation *counterDeviceAlloc) {
    NEO::GraphicsAllocation *counterDeviceAllocForResidency = counterDeviceAlloc;

    if (counterDeviceAllocForResidency && (counterDeviceAllocForResidency->getRootDeviceIndex() != device->getRootDeviceIndex())) {
        DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());

        counterDeviceAllocForResidency = driverHandle->getCounterPeerAllocation(device, *counterDeviceAllocForResidency);
        UNRECOVERABLE_IF(!counterDeviceAllocForResidency);
        UNRECOVERABLE_IF(counterDeviceAllocForResidency->getGpuAddress() != counterDeviceAlloc->getGpuAddress());
    }
    return counterDeviceAllocForResidency;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendWaitOnInOrderDependency(std::shared_ptr<NEO::InOrderExecInfo> &inOrderExecInfo, CommandToPatchContainer *outListCommands,
                                                                         uint64_t waitValue, uint32_t offset, bool relaxedOrderingAllowed, bool implicitDependency, bool skipAddingWaitEventsToResidency,
                                                                         bool noopDispatch, bool dualStreamCopyOffloadOperation) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    UNRECOVERABLE_IF(waitValue > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) && !isQwordInOrderCounter());

    auto deviceAllocForResidency = this->getDeviceCounterAllocForResidency(inOrderExecInfo->getDeviceCounterAllocation());
    if (!skipAddingWaitEventsToResidency) {
        commandContainer.addToResidencyContainer(deviceAllocForResidency);
    }

    uint64_t gpuAddress = inOrderExecInfo->getBaseDeviceAddress() + offset;

    const uint32_t immWriteOffset = device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
    const bool copyOnlyWait = isCopyOnly(dualStreamCopyOffloadOperation);

    for (uint32_t i = 0; i < inOrderExecInfo->getNumDevicePartitionsToWait(); i++) {
        if (relaxedOrderingAllowed) {
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(*commandContainer.getCommandStream(), 0, gpuAddress, waitValue, NEO::CompareOperation::less, true,
                                                                                                   isQwordInOrderCounter(), copyOnlyWait);

        } else {
            auto resolveDependenciesViaPipeControls = !copyOnlyWait && implicitDependency && (this->dcFlushSupport || (!this->heaplessModeEnabled && this->latestOperationHasOptimizedCbEvent));

            if (NEO::debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
                resolveDependenciesViaPipeControls = NEO::debugManager.flags.ResolveDependenciesViaPipeControls.get();
            }

            if (resolveDependenciesViaPipeControls) {
                NEO::PipeControlArgs args;
                if (this->consumeTextureCacheFlushPending()) {
                    args.textureCacheInvalidationEnable = true;
                } else {
                    args.csStallOnly = true;
                }
                NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
                break;
            } else {
                using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
                using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

                bool indirectMode = false;

                size_t inOrderPatchListIndex = std::numeric_limits<size_t>::max();

                bool patchingRequired = inOrderExecInfo->isRegularCmdList() && !inOrderExecInfo->isExternalMemoryExecInfo();

                if (isQwordInOrderCounter()) {
                    indirectMode = true;

                    constexpr uint32_t firstRegister = RegisterOffsets::csGprR0;
                    constexpr uint32_t secondRegister = RegisterOffsets::csGprR0 + 4;

                    auto lri1 = commandContainer.getCommandStream()->template getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
                    auto lri2 = commandContainer.getCommandStream()->template getSpaceForCmd<MI_LOAD_REGISTER_IMM>();

                    if (!noopDispatch) {
                        NEO::LriHelper<GfxFamily>::program(lri1, firstRegister, getLowPart(waitValue), true, copyOnlyWait);
                        NEO::LriHelper<GfxFamily>::program(lri2, secondRegister, getHighPart(waitValue), true, copyOnlyWait);
                    } else {
                        memset(lri1, 0, sizeof(MI_LOAD_REGISTER_IMM));
                        memset(lri2, 0, sizeof(MI_LOAD_REGISTER_IMM));
                    }

                    if (patchingRequired) {
                        inOrderPatchListIndex = addCmdForPatching((implicitDependency ? nullptr : &inOrderExecInfo), lri1, lri2, waitValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::lri64b);
                        if (noopDispatch) {
                            disablePatching(inOrderPatchListIndex);
                        }
                    }
                    if (outListCommands != nullptr) {
                        auto &lri1ToPatch = outListCommands->emplace_back();
                        lri1ToPatch.type = CommandToPatch::CbWaitEventLoadRegisterImm;
                        lri1ToPatch.pDestination = lri1;
                        lri1ToPatch.inOrderPatchListIndex = inOrderPatchListIndex;
                        lri1ToPatch.offset = firstRegister;

                        auto &lri2ToPatch = outListCommands->emplace_back();
                        lri2ToPatch.type = CommandToPatch::CbWaitEventLoadRegisterImm;
                        lri2ToPatch.pDestination = lri2;
                        lri2ToPatch.inOrderPatchListIndex = inOrderPatchListIndex;
                        lri2ToPatch.offset = secondRegister;
                    }
                }

                auto semaphoreCommand = reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandContainer.getCommandStream()->getSpace(sizeof(MI_SEMAPHORE_WAIT)));

                if (!noopDispatch) {
                    NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(semaphoreCommand, gpuAddress, waitValue, COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                                            false, true, isQwordInOrderCounter(), indirectMode, false);
                } else {
                    memset(semaphoreCommand, 0, sizeof(MI_SEMAPHORE_WAIT));
                }

                if (patchingRequired && !isQwordInOrderCounter()) {
                    inOrderPatchListIndex = addCmdForPatching((implicitDependency ? nullptr : &inOrderExecInfo), semaphoreCommand, nullptr, waitValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::semaphore);
                    if (noopDispatch) {
                        disablePatching(inOrderPatchListIndex);
                    }
                } else {
                    inOrderPatchListIndex = std::numeric_limits<size_t>::max();
                }

                if (outListCommands != nullptr) {
                    auto &semaphoreWaitPatch = outListCommands->emplace_back();
                    semaphoreWaitPatch.type = CommandToPatch::CbWaitEventSemaphoreWait;
                    semaphoreWaitPatch.pDestination = semaphoreCommand;
                    semaphoreWaitPatch.offset = i * immWriteOffset;
                    semaphoreWaitPatch.inOrderPatchListIndex = inOrderPatchListIndex;
                }
            }
        }

        gpuAddress += immWriteOffset;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::canSkipInOrderEventWait(Event &event, bool ignorCbEventBoundToCmdList) const {
    if (isInOrderExecutionEnabled()) {
        return ((isImmediateType() && event.getLatestUsedCmdQueue() == this->cmdQImmediate) || // 1. Immediate CmdList can skip "regular Events" from the same CmdList
                (isCbEventBoundToCmdList(&event) && !ignorCbEventBoundToCmdList));             // 2. Both Immediate and Regular CmdLists can skip "CounterBased Events" from the same CmdList
    }

    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                                                     bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) {
    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::debugManager.flags.EnableSWTags.get()) {
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->incrementAndGetCurrentCallCount();
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendWaitOnEvents",
            callId);
    }

    const bool dualStreamCopyOffload = isDualStreamCopyOffloadOperation(copyOffloadOperation);

    if (this->isInOrderExecutionEnabled() && apiRequest) {
        handleInOrderImplicitDependencies(false, dualStreamCopyOffload);
    }

    bool dcFlushRequired = false;

    if (this->dcFlushSupport) {
        for (uint32_t i = 0; i < numEvents; i++) {
            auto event = Event::fromHandle(phEvent[i]);
            dcFlushRequired |= event->isWaitScope();
        }
    }
    if (dcFlushRequired) {
        UNRECOVERABLE_IF(isNonDualStreamCopyOffloadOperation(copyOffloadOperation));
        if (isCopyOnly(copyOffloadOperation)) {
            NEO::MiFlushArgs args{this->dummyBlitWa};
            encodeMiFlush(0, 0, args);
        } else if (!this->l3FlushAfterPostSyncRequired) {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = true;
            args.textureCacheInvalidationEnable = this->consumeTextureCacheFlushPending();
            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
        }
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phEvent[i]);

        if (event->isCounterBased() && !event->getInOrderExecInfo().get()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT; // in-order event not signaled yet
        }

        if ((isImmediateType() && event->isAlreadyCompleted()) ||
            canSkipInOrderEventWait(*event, this->allowCbWaitEventsNoopDispatch)) {
            continue;
        }

        if (event->isCounterBased() && (this->heaplessModeEnabled || !event->hasInOrderTimestampNode())) {
            // 1. Regular CmdList adds submission counter to base value on each Execute
            // 2. Immediate CmdList takes current value (with submission counter)
            auto waitValue = !isImmediateType() ? event->getInOrderExecBaseSignalValue() : event->getInOrderExecSignalValueWithSubmissionCounter();

            CommandListCoreFamily<gfxCoreFamily>::appendWaitOnInOrderDependency(event->getInOrderExecInfo(), outWaitCmds,
                                                                                waitValue, event->getInOrderAllocationOffset(),
                                                                                relaxedOrderingAllowed, false, skipAddingWaitEventsToResidency,
                                                                                isCbEventBoundToCmdList(event), dualStreamCopyOffload);

            continue;
        }

        if (!isImmediateType()) {
            event->disableImplicitCounterBasedMode();
        }

        if (!skipAddingWaitEventsToResidency) {
            commandContainer.addToResidencyContainer(event->getAllocation(this->device));
        }

        appendWaitOnSingleEvent(event, outWaitCmds, relaxedOrderingAllowed, dualStreamCopyOffload, CommandToPatch::WaitEventSemaphoreWait);
    }

    if (isImmediateType() && isCopyOnly(dualStreamCopyOffload) && trackDependencies) {
        NEO::MiFlushArgs args{this->dummyBlitWa};
        args.commandWithPostSync = true;
        auto csr = getCsr(false);
        encodeMiFlush(csr->getBarrierCountGpuAddress(), csr->getNextBarrierCount() + 1, args);
        commandContainer.addToResidencyContainer(csr->getTagAllocation());
    }

    if (apiRequest) {
        if (this->isInOrderExecutionEnabled()) {
            appendSignalInOrderDependencyCounter(nullptr, false, false, false);
        }
        handleInOrderDependencyCounter(nullptr, false, false);
    }

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendWaitOnEvents",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSdiInOrderCounterSignalling(uint64_t baseGpuVa, uint64_t signalValue, bool copyOffloadOperation) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    UNRECOVERABLE_IF(isNonDualStreamCopyOffloadOperation(copyOffloadOperation));

    uint64_t gpuVa = baseGpuVa + inOrderExecInfo->getAllocationOffset();

    uint32_t numWrites = 1;
    bool partitionOffsetEnabled = this->partitionCount > 1;

    if (copyOffloadOperation && partitionOffsetEnabled) {
        numWrites = this->partitionCount;
        partitionOffsetEnabled = false;
    }

    for (uint32_t i = 0; i < numWrites; i++) {
        auto miStoreCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(commandContainer.getCommandStream()->getSpace(sizeof(MI_STORE_DATA_IMM)));

        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(miStoreCmd, gpuVa, getLowPart(signalValue), getHighPart(signalValue),
                                                               isQwordInOrderCounter(), partitionOffsetEnabled);

        addCmdForPatching(nullptr, miStoreCmd, nullptr, signalValue, NEO::InOrderPatchCommandHelpers::PatchCmdType::sdi);

        gpuVa += device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSignalInOrderDependencyCounter(Event *signalEvent, bool copyOffloadOperation, bool stall, bool textureFlushRequired) {
    using ATOMIC_OPCODES = typename GfxFamily::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename GfxFamily::MI_ATOMIC::DATA_SIZE;

    UNRECOVERABLE_IF(isNonDualStreamCopyOffloadOperation(copyOffloadOperation));

    uint64_t deviceAllocGpuVa = inOrderExecInfo->getBaseDeviceAddress();
    uint64_t signalValue = inOrderExecInfo->getCounterValue() + getInOrderIncrementValue();

    auto cmdStream = commandContainer.getCommandStream();

    if (stall) {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = true;
        args.workloadPartitionOffset = partitionCount > 1;
        args.textureCacheInvalidationEnable = textureFlushRequired;

        NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *cmdStream,
            NEO::PostSyncMode::immediateData,
            deviceAllocGpuVa + inOrderExecInfo->getAllocationOffset(),
            signalValue,
            device->getNEODevice()->getRootDeviceEnvironment(),
            args);

    } else if (this->inOrderAtomicSignalingEnabled) {
        ATOMIC_OPCODES opcode = ATOMIC_OPCODES::ATOMIC_8B_INCREMENT;
        uint64_t operand1Data = 0;

        if (copyOffloadOperation && this->partitionCount > 1) {
            opcode = ATOMIC_OPCODES::ATOMIC_8B_ADD;
            operand1Data = this->partitionCount;
        }

        NEO::EncodeAtomic<GfxFamily>::programMiAtomic(*cmdStream, deviceAllocGpuVa, opcode,
                                                      DATA_SIZE::DATA_SIZE_QWORD, 0, 0, operand1Data, 0);

    } else {
        appendSdiInOrderCounterSignalling(deviceAllocGpuVa, signalValue, copyOffloadOperation);
    }

    if (inOrderExecInfo->isHostStorageDuplicated()) {
        appendSdiInOrderCounterSignalling(inOrderExecInfo->getBaseHostGpuAddress(), signalValue, copyOffloadOperation);
    }

    if (signalEvent && signalEvent->getInOrderIncrementValue() > 0) {
        NEO::EncodeAtomic<GfxFamily>::programMiAtomic(*cmdStream, signalEvent->getInOrderExecInfo()->getBaseDeviceAddress(), ATOMIC_OPCODES::ATOMIC_8B_ADD,
                                                      DATA_SIZE::DATA_SIZE_QWORD, 0, 0, signalEvent->getInOrderIncrementValue(), 0);
    }

    if ((NEO::debugManager.flags.ProgramUserInterruptOnResolvedDependency.get() == 1 || isCopyOnly(copyOffloadOperation)) && signalEvent && signalEvent->isInterruptModeEnabled()) {
        NEO::EnodeUserInterrupt<GfxFamily>::encode(*cmdStream);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::programSyncBuffer(Kernel &kernel, NEO::Device &device,
                                                                    const ze_group_count_t &threadGroupDimensions, size_t &patchIndex) {
    uint32_t maximalNumberOfWorkgroupsAllowed = kernel.suggestMaxCooperativeGroupCount(this->engineGroupType, false);

    size_t requestedNumberOfWorkgroups = (threadGroupDimensions.groupCountX * threadGroupDimensions.groupCountY * threadGroupDimensions.groupCountZ);
    if (requestedNumberOfWorkgroups > maximalNumberOfWorkgroupsAllowed) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto patchData = NEO::KernelHelper::getSyncBufferAllocationOffset(device, requestedNumberOfWorkgroups);
    kernel.patchSyncBuffer(patchData.first, patchData.second);

    if (!isImmediateType()) {
        patchIndex = commandsToPatch.size();

        CommandToPatch syncBufferSpace;
        syncBufferSpace.type = CommandToPatch::NoopSpace;
        syncBufferSpace.offset = patchData.second;
        syncBufferSpace.pDestination = ptrOffset(patchData.first->getUnderlyingBuffer(), patchData.second);
        syncBufferSpace.patchSize = NEO::KernelHelper::getSyncBufferSize(requestedNumberOfWorkgroups);

        commandsToPatch.push_back(syncBufferSpace);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programRegionGroupBarrier(Kernel &kernel, const ze_group_count_t &threadGroupDimensions, size_t localRegionSize, size_t &patchIndex) {
    auto neoDevice = device->getNEODevice();

    auto threadGroupCount = threadGroupDimensions.groupCountX * threadGroupDimensions.groupCountY * threadGroupDimensions.groupCountZ;
    auto patchData = NEO::KernelHelper::getRegionGroupBarrierAllocationOffset(*neoDevice, threadGroupCount, localRegionSize);

    kernel.patchRegionGroupBarrier(patchData.first, patchData.second);

    if (!isImmediateType()) {
        patchIndex = commandsToPatch.size();

        CommandToPatch regionBarrierSpace;
        regionBarrierSpace.type = CommandToPatch::NoopSpace;
        regionBarrierSpace.offset = patchData.second;
        regionBarrierSpace.pDestination = ptrOffset(patchData.first->getUnderlyingBuffer(), patchData.second);
        regionBarrierSpace.patchSize = NEO::KernelHelper::getRegionGroupBarrierSize(threadGroupCount, localRegionSize);

        commandsToPatch.push_back(regionBarrierSpace);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendWriteKernelTimestamp(Event *event, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool maskLsb, bool workloadPartition, bool copyOperation) {
    constexpr uint32_t mask = 0xfffffffe;

    auto baseAddr = event->getPacketAddress(this->device);

    auto globalOffset = beforeWalker ? event->getGlobalStartOffset() : event->getGlobalEndOffset();
    auto contextOffset = beforeWalker ? event->getContextStartOffset() : event->getContextEndOffset();

    void **globalPostSyncCmdBuffer = nullptr;
    void **contextPostSyncCmdBuffer = nullptr;

    void *globalPostSyncCmd = nullptr;
    void *contextPostSyncCmd = nullptr;

    if (outTimeStampSyncCmds != nullptr) {
        globalPostSyncCmdBuffer = &globalPostSyncCmd;
        contextPostSyncCmdBuffer = &contextPostSyncCmd;
    }

    uint64_t globalAddress = ptrOffset(baseAddr, globalOffset);
    uint64_t contextAddress = ptrOffset(baseAddr, contextOffset);

    if (maskLsb) {
        NEO::EncodeMathMMIO<GfxFamily>::encodeBitwiseAndVal(commandContainer, RegisterOffsets::globalTimestampLdw, mask, globalAddress, workloadPartition, globalPostSyncCmdBuffer, copyOperation);
        NEO::EncodeMathMMIO<GfxFamily>::encodeBitwiseAndVal(commandContainer, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, mask, contextAddress, workloadPartition, contextPostSyncCmdBuffer, copyOperation);
    } else {
        NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), RegisterOffsets::globalTimestampLdw, globalAddress, workloadPartition, globalPostSyncCmdBuffer, copyOperation);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextAddress, workloadPartition, contextPostSyncCmdBuffer, copyOperation);
    }

    if (outTimeStampSyncCmds != nullptr) {
        CommandToPatch ctxCmd;
        ctxCmd.type = CommandToPatch::TimestampEventPostSyncStoreRegMem;

        ctxCmd.offset = globalOffset;
        ctxCmd.pDestination = globalPostSyncCmd;
        outTimeStampSyncCmds->push_back(ctxCmd);

        ctxCmd.offset = contextOffset;
        ctxCmd.pDestination = contextPostSyncCmd;
        outTimeStampSyncCmds->push_back(ctxCmd);
    }

    adjustWriteKernelTimestamp(globalAddress, contextAddress, baseAddr, outTimeStampSyncCmds, workloadPartition, copyOperation);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfiling(Event *event, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool skipBarrierForEndProfiling, bool skipAddingEventToResidency, bool copyOperation) {
    if (!event) {
        return;
    }

    if (copyOperation) {
        appendEventForProfilingCopyCommand(event, beforeWalker);
    } else {
        if (!event->isEventTimestampFlagSet()) {
            return;
        }

        if (!skipAddingEventToResidency) {
            commandContainer.addToResidencyContainer(event->getAllocation(this->device));
        }
        bool workloadPartition = isTimestampEventForMultiTile(event);

        appendDispatchOffsetRegister(workloadPartition, true);

        if (beforeWalker) {
            event->resetKernelCountAndPacketUsedCount();
            bool workloadPartition = setupTimestampEventForMultiTile(event);
            appendWriteKernelTimestamp(event, outTimeStampSyncCmds, beforeWalker, true, workloadPartition, copyOperation);
        } else {
            dispatchEventPostSyncOperation(event, nullptr, nullptr, Event::STATE_SIGNALED, true, false, false, true, copyOperation);

            const auto &rootDeviceEnvironment = this->device->getNEODevice()->getRootDeviceEnvironment();

            if (!skipBarrierForEndProfiling) {
                NEO::PipeControlArgs args;
                args.dcFlushEnable = getDcFlushRequired(event->isSignalScope());
                args.textureCacheInvalidationEnable = this->consumeTextureCacheFlushPending();
                NEO::MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(args);

                NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
            }

            uint64_t baseAddr = event->getGpuAddress(this->device);
            NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(*commandContainer.getCommandStream(), baseAddr, NEO::FenceType::release, rootDeviceEnvironment);
            appendWriteKernelTimestamp(event, outTimeStampSyncCmds, beforeWalker, true, workloadPartition, copyOperation);
        }

        appendDispatchOffsetRegister(workloadPartition, false);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, false, true, true, false, false);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(signalEvent, nullptr, true, false, false, isCopyOnly(false));

    auto allocationStruct = getAlignedAllocationData(this->device, false, dstptr, sizeof(uint64_t), false, false);
    if (allocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    commandContainer.addToResidencyContainer(allocationStruct.alloc);

    if (isCopyOnly(false)) {
        NEO::MiFlushArgs args{this->dummyBlitWa};
        args.timeStampOperation = true;
        args.commandWithPostSync = true;
        encodeMiFlush(allocationStruct.alignedAllocationPtr, 0, args);
    } else {
        NEO::PipeControlArgs args;
        args.blockSettingPostSyncProperties = true;

        NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *commandContainer.getCommandStream(),
            NEO::PostSyncMode::timestamp,
            allocationStruct.alignedAllocationPtr,
            0,
            this->device->getNEODevice()->getRootDeviceEnvironment(),
            args);
    }

    appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, isCopyOnly(false));

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
    }
    handleInOrderDependencyCounter(signalEvent, false, false);

    appendSynchronizedDispatchCleanupSection();

    addToMappedEventList(signalEvent);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    memoryCopyParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendQueryKernelTimestamps(
    uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr,
    const size_t *pOffsets, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto dstPtrAllocationStruct = getAlignedAllocationData(this->device, false, dstptr, sizeof(ze_kernel_timestamp_result_t) * numEvents, false, false);
    if (dstPtrAllocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    commandContainer.addToResidencyContainer(dstPtrAllocationStruct.alloc);

    std::unique_ptr<EventData[]> timestampsData = std::make_unique_for_overwrite<EventData[]>(numEvents);

    for (uint32_t i = 0u; i < numEvents; ++i) {
        auto event = Event::fromHandle(phEvents[i]);
        commandContainer.addToResidencyContainer(event->getAllocation(this->device));
        timestampsData[i].address = event->getGpuAddress(this->device);
        timestampsData[i].packetsInUse = event->getPacketsInUse();
        timestampsData[i].timestampSizeInDw = event->getTimestampSizeInDw();
    }

    size_t alignedSize = alignUp<size_t>(sizeof(EventData) * numEvents, MemoryConstants::pageSize64k);
    NEO::AllocationType allocationType = NEO::AllocationType::gpuTimestampDeviceBuffer;
    auto devices = device->getNEODevice()->getDeviceBitfield();
    NEO::AllocationProperties allocationProperties{device->getRootDeviceIndex(),
                                                   true,
                                                   alignedSize,
                                                   allocationType,
                                                   devices.count() > 1,
                                                   false,
                                                   devices};

    NEO::GraphicsAllocation *timestampsGPUData = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocationProperties);

    UNRECOVERABLE_IF(timestampsGPUData == nullptr);

    commandContainer.addToResidencyContainer(timestampsGPUData);
    commandContainer.getDeallocationContainer().push_back(timestampsGPUData);

    bool result = device->getDriverHandle()->getMemoryManager()->copyMemoryToAllocation(timestampsGPUData, 0, timestampsData.get(), sizeof(EventData) * numEvents);

    UNRECOVERABLE_IF(!result);

    Kernel *builtinKernel = nullptr;
    auto useOnlyGlobalTimestampsValue = this->useOnlyGlobalTimestamps ? 1u : 0u;
    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    if (pOffsets == nullptr) {
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps);
        builtinKernel->setArgumentValue(2u, sizeof(uint32_t), &useOnlyGlobalTimestampsValue);
    } else {
        auto pOffsetAllocationStruct = getAlignedAllocationData(this->device, false, pOffsets, sizeof(size_t) * numEvents, false, false);
        if (pOffsetAllocationStruct.alloc == nullptr) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        auto offsetValPtr = static_cast<uintptr_t>(pOffsetAllocationStruct.alloc->getGpuAddress());
        commandContainer.addToResidencyContainer(pOffsetAllocationStruct.alloc);
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestampsWithOffsets);
        builtinKernel->setArgBufferWithAlloc(2, offsetValPtr, pOffsetAllocationStruct.alloc, nullptr);
        builtinKernel->setArgumentValue(3u, sizeof(uint32_t), &useOnlyGlobalTimestampsValue);
        offsetValPtr += sizeof(size_t);
    }

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    ze_result_t ret = builtinKernel->suggestGroupSize(numEvents, 1u, 1u,
                                                      &groupSizeX, &groupSizeY, &groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    ret = builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    if (ret != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ret;
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phEvents[i]);
        if (event->isCounterBased()) {
            appendWaitOnSingleEvent(event, nullptr, false, false, CommandToPatch::CbEventTimestampPostSyncSemaphoreWait);
        }
    }

    ze_group_count_t dispatchKernelArgs{numEvents / groupSizeX, 1u, 1u};

    auto dstValPtr = static_cast<uintptr_t>(dstPtrAllocationStruct.alloc->getGpuAddress());

    builtinKernel->setArgBufferWithAlloc(0u, static_cast<uintptr_t>(timestampsGPUData->getGpuAddress()), timestampsGPUData, nullptr);
    builtinKernel->setArgBufferWithAlloc(1, dstValPtr, dstPtrAllocationStruct.alloc, nullptr);

    auto dstAllocationType = dstPtrAllocationStruct.alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::bufferHostMemory) ||
        (dstAllocationType == NEO::AllocationType::externalHostPtr);

    if constexpr (checkIfAllocationImportedRequired()) {
        launchParams.isDestinationAllocationImported = this->isAllocationImported(dstPtrAllocationStruct.alloc, device->getDriverHandle()->getSvmAllocsManager());
    }
    auto appendResult = appendLaunchKernel(builtinKernel->toHandle(), dispatchKernelArgs, hSignalEvent, numWaitEvents,
                                           phWaitEvents, launchParams);
    if (appendResult != ZE_RESULT_SUCCESS) {
        return appendResult;
    }

    addToMappedEventList(Event::fromHandle(hSignalEvent));

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::hostSynchronize(uint64_t timeout) {
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::reserveSpace(size_t size, void **ptr) {
    auto availableSpace = commandContainer.getCommandStream()->getAvailableSpace();
    if (availableSpace < size) {
        *ptr = nullptr;
    } else {
        *ptr = commandContainer.getCommandStream()->getSpace(size);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::prepareIndirectParams(const ze_group_count_t *threadGroupDimensions) {
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(static_cast<const void *>(threadGroupDimensions));
    if (allocData) {
        auto alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        commandContainer.addToResidencyContainer(alloc);

        size_t groupCountOffset = 0;
        if (allocData->cpuAllocation != nullptr) {
            commandContainer.addToResidencyContainer(allocData->cpuAllocation);
            groupCountOffset = ptrDiff(threadGroupDimensions, allocData->cpuAllocation->getUnderlyingBuffer());
        } else {
            groupCountOffset = ptrDiff(threadGroupDimensions, alloc->getGpuAddress());
        }

        auto groupCount = ptrOffset(alloc->getGpuAddress(), groupCountOffset);
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, RegisterOffsets::gpgpuDispatchDimX,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountX)), isCopyOnly(false));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, RegisterOffsets::gpgpuDispatchDimY,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountY)), isCopyOnly(false));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, RegisterOffsets::gpgpuDispatchDimZ,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountZ)), isCopyOnly(false));
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::updateStreamProperties(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect) {
    if (isImmediateType()) {
        updateStreamPropertiesForFlushTaskDispatchFlags(kernel, isCooperative, threadGroupDimensions, isIndirect);
    } else {
        updateStreamPropertiesForRegularCommandLists(kernel, isCooperative, threadGroupDimensions, isIndirect);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline bool getFusedEuDisabled(Kernel &kernel, Device *device, const ze_group_count_t &threadGroupDimensions, bool isIndirect) {
    auto &kernelAttributes = kernel.getKernelDescriptor().kernelAttributes;

    bool fusedEuDisabled = kernelAttributes.flags.requiresDisabledEUFusion;
    if (static_cast<DeviceImp *>(device)->calculationForDisablingEuFusionWithDpasNeeded) {
        auto &productHelper = device->getProductHelper();
        uint32_t *groupCountPtr = nullptr;
        uint32_t groupCount[3] = {};
        if (!isIndirect) {
            groupCount[0] = threadGroupDimensions.groupCountX;
            groupCount[1] = threadGroupDimensions.groupCountY;
            groupCount[2] = threadGroupDimensions.groupCountZ;
            groupCountPtr = groupCount;
        }
        fusedEuDisabled |= productHelper.isFusedEuDisabledForDpas(kernelAttributes.flags.usesSystolicPipelineSelectMode, kernel.getGroupSize(), groupCountPtr, device->getHwInfo());
    }
    return fusedEuDisabled;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::updateStreamPropertiesForFlushTaskDispatchFlags(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect) {
    auto &kernelAttributes = kernel.getKernelDescriptor().kernelAttributes;

    bool fusedEuDisabled = getFusedEuDisabled<gfxCoreFamily>(kernel, this->device, threadGroupDimensions, isIndirect);

    requiredStreamState.stateComputeMode.setPropertiesGrfNumberThreadArbitration(kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy);

    requiredStreamState.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperative, fusedEuDisabled);

    requiredStreamState.pipelineSelect.setPropertySystolicMode(kernelAttributes.flags.usesSystolicPipelineSelectMode);

    KernelImp &kernelImp = static_cast<KernelImp &>(kernel);
    int32_t currentMocsState = static_cast<int32_t>(device->getMOCS(!kernelImp.getKernelRequiresUncachedMocs(), false) >> 1);
    requiredStreamState.stateBaseAddress.setPropertyStatelessMocs(currentMocsState);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendVfeStateCmdToPatch() {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
        using FrontEndStateCommand = typename GfxFamily::FrontEndStateCommand;
        auto frontEndStateAddress = NEO::PreambleHelper<GfxFamily>::getSpaceForVfeState(commandContainer.getCommandStream(), device->getHwInfo(), engineGroupType);
        auto frontEndStateCmd = new FrontEndStateCommand;
        NEO::PreambleHelper<GfxFamily>::programVfeState(frontEndStateCmd, rootDeviceEnvironment, 0, 0, device->getMaxNumHwThreads(), finalStreamState);
        commandsToPatch.push_back({.pDestination = frontEndStateAddress,
                                   .pCommand = frontEndStateCmd,
                                   .type = CommandToPatch::FrontEndState});
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::updateStreamPropertiesForRegularCommandLists(Kernel &kernel, bool isCooperative, const ze_group_count_t &threadGroupDimensions, bool isIndirect) {
    size_t currentSurfaceStateSize = NEO::StreamPropertySizeT::initValue;
    size_t currentDynamicStateSize = NEO::StreamPropertySizeT::initValue;
    size_t currentIndirectObjectSize = NEO::StreamPropertySizeT::initValue;
    size_t currentBindingTablePoolSize = NEO::StreamPropertySizeT::initValue;

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    auto &kernelAttributes = kernel.getKernelDescriptor().kernelAttributes;

    const auto bindlessHeapsHelper = device->getNEODevice()->getBindlessHeapsHelper();
    const bool useGlobalHeaps = bindlessHeapsHelper != nullptr;

    KernelImp &kernelImp = static_cast<KernelImp &>(kernel);

    int32_t currentMocsState = static_cast<int32_t>(device->getMOCS(!kernelImp.getKernelRequiresUncachedMocs(), false) >> 1);
    bool checkSsh = false;
    bool checkDsh = false;
    bool checkIoh = false;

    if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) {
        if (currentSurfaceStateBaseAddress == NEO::StreamProperty64::initValue || commandContainer.isHeapDirty(NEO::IndirectHeap::Type::surfaceState)) {
            auto ssh = commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::surfaceState);
            if (ssh) {
                currentSurfaceStateBaseAddress = NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps);
                currentSurfaceStateSize = NEO::getStateSizeForSsh(*ssh, useGlobalHeaps);

                currentBindingTablePoolBaseAddress = currentSurfaceStateBaseAddress;
                currentBindingTablePoolSize = currentSurfaceStateSize;

                checkSsh = true;
            }
            DEBUG_BREAK_IF(ssh == nullptr && commandContainer.isHeapDirty(NEO::IndirectHeap::Type::surfaceState));
        }

        if (this->dynamicHeapRequired && (currentDynamicStateBaseAddress == NEO::StreamProperty64::initValue || commandContainer.isHeapDirty(NEO::IndirectHeap::Type::dynamicState))) {
            auto dsh = commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::dynamicState);

            currentDynamicStateBaseAddress = NEO::getStateBaseAddress(*dsh, useGlobalHeaps);
            currentDynamicStateSize = NEO::getStateSize(*dsh, useGlobalHeaps);

            checkDsh = true;
        }
    }

    if (currentIndirectObjectBaseAddress == NEO::StreamProperty64::initValue) {
        auto ioh = commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::indirectObject);
        currentIndirectObjectBaseAddress = ioh->getHeapGpuBase();
        currentIndirectObjectSize = ioh->getHeapSizeInPages();

        checkIoh = true;
    }

    bool fusedEuDisabled = getFusedEuDisabled<gfxCoreFamily>(kernel, this->device, threadGroupDimensions, isIndirect);

    if (!containsAnyKernel) {
        requiredStreamState.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperative, fusedEuDisabled);
        requiredStreamState.pipelineSelect.setPropertySystolicMode(kernelAttributes.flags.usesSystolicPipelineSelectMode);

        requiredStreamState.stateBaseAddress.setPropertyStatelessMocs(currentMocsState);

        if (checkSsh) {
            requiredStreamState.stateBaseAddress.setPropertiesBindingTableSurfaceState(currentBindingTablePoolBaseAddress, currentBindingTablePoolSize,
                                                                                       currentSurfaceStateBaseAddress, currentSurfaceStateSize);
        }
        if (checkDsh) {
            requiredStreamState.stateBaseAddress.setPropertiesDynamicState(currentDynamicStateBaseAddress, currentDynamicStateSize);
        }
        requiredStreamState.stateBaseAddress.setPropertiesIndirectState(currentIndirectObjectBaseAddress, currentIndirectObjectSize);

        if (this->stateComputeModeTracking) {
            requiredStreamState.stateComputeMode.setPropertiesGrfNumberThreadArbitration(kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy);
            finalStreamState = requiredStreamState;
        } else {
            finalStreamState = requiredStreamState;
            requiredStreamState.stateComputeMode.setPropertiesAll(cmdListDefaultCoherency, kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy, device->getDevicePreemptionMode());
        }
        containsAnyKernel = true;
    }

    finalStreamState.pipelineSelect.setPropertySystolicMode(kernelAttributes.flags.usesSystolicPipelineSelectMode);
    if (this->pipelineSelectStateTracking && finalStreamState.pipelineSelect.isDirty()) {
        NEO::PipelineSelectArgs pipelineSelectArgs;
        pipelineSelectArgs.systolicPipelineSelectMode = kernelAttributes.flags.usesSystolicPipelineSelectMode;
        pipelineSelectArgs.systolicPipelineSelectSupport = this->systolicModeSupport;

        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(commandContainer.getCommandStream(),
                                                              pipelineSelectArgs,
                                                              rootDeviceEnvironment);
    }

    finalStreamState.frontEndState.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperative, fusedEuDisabled);
    bool isPatchingVfeStateAllowed = (NEO::debugManager.flags.AllowPatchingVfeStateInCommandLists.get() || (this->frontEndStateTracking && this->dispatchCmdListBatchBufferAsPrimary));
    if (finalStreamState.frontEndState.isDirty()) {
        if (isPatchingVfeStateAllowed) {
            appendVfeStateCmdToPatch();
        }

        if (this->frontEndStateTracking && !this->dispatchCmdListBatchBufferAsPrimary) {
            auto &stream = *commandContainer.getCommandStream();
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferEnd(stream);

            CmdListReturnPoint returnPoint = {
                {},
                stream.getGpuBase() + stream.getUsed(),
                stream.getGraphicsAllocation()};
            returnPoint.configSnapshot.frontEndState.copyPropertiesAll(finalStreamState.frontEndState);
            returnPoints.push_back(returnPoint);
        }
    }

    if (this->stateComputeModeTracking) {
        finalStreamState.stateComputeMode.setPropertiesGrfNumberThreadArbitration(kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy);
    } else {
        finalStreamState.stateComputeMode.setPropertiesAll(cmdListDefaultCoherency, kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy, device->getDevicePreemptionMode());
    }
    if (finalStreamState.stateComputeMode.isDirty()) {
        bool isRcs = (this->engineGroupType == NEO::EngineGroupType::renderCompute);
        NEO::PipelineSelectArgs pipelineSelectArgs;
        pipelineSelectArgs.systolicPipelineSelectMode = kernelAttributes.flags.usesSystolicPipelineSelectMode;
        pipelineSelectArgs.systolicPipelineSelectSupport = this->systolicModeSupport;

        NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(
            *commandContainer.getCommandStream(), finalStreamState.stateComputeMode, pipelineSelectArgs, false, rootDeviceEnvironment, isRcs, this->dcFlushSupport);
    }

    finalStreamState.stateBaseAddress.setPropertyStatelessMocs(currentMocsState);
    if (checkSsh) {
        finalStreamState.stateBaseAddress.setPropertiesBindingTableSurfaceState(currentBindingTablePoolBaseAddress, currentBindingTablePoolSize,
                                                                                currentSurfaceStateBaseAddress, currentSurfaceStateSize);
    }
    if (checkDsh) {
        finalStreamState.stateBaseAddress.setPropertiesDynamicState(currentDynamicStateBaseAddress, currentDynamicStateSize);
    }
    if (checkIoh) {
        finalStreamState.stateBaseAddress.setPropertiesIndirectState(currentIndirectObjectBaseAddress, currentIndirectObjectSize);
    }

    if (this->stateBaseAddressTracking && finalStreamState.stateBaseAddress.isDirty()) {
        commandContainer.setDirtyStateForAllHeaps(false);
        programStateBaseAddress(commandContainer, true);
        finalStreamState.stateBaseAddress.clearIsDirty();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::clearCommandsToPatch() {
    if constexpr (GfxFamily::isHeaplessRequired()) {
        for (auto &commandToPatch : commandsToPatch) {
            switch (commandToPatch.type) {
            case CommandToPatch::PauseOnEnqueueSemaphoreStart:
            case CommandToPatch::PauseOnEnqueueSemaphoreEnd:
            case CommandToPatch::PauseOnEnqueuePipeControlStart:
            case CommandToPatch::PauseOnEnqueuePipeControlEnd:
                UNRECOVERABLE_IF(commandToPatch.pCommand == nullptr);
                break;
            case CommandToPatch::ComputeWalkerInlineDataScratch:
            case CommandToPatch::ComputeWalkerImplicitArgsScratch:
            case CommandToPatch::NoopSpace:
                break;
            default:
                UNRECOVERABLE_IF(true);
            }
        }
        commandsToPatch.clear();
    } else {
        using FrontEndStateCommand = typename GfxFamily::FrontEndStateCommand;

        for (auto &commandToPatch : commandsToPatch) {
            switch (commandToPatch.type) {
            case CommandToPatch::FrontEndState:
                UNRECOVERABLE_IF(commandToPatch.pCommand == nullptr);
                delete reinterpret_cast<FrontEndStateCommand *>(commandToPatch.pCommand);
                break;
            case CommandToPatch::PauseOnEnqueueSemaphoreStart:
            case CommandToPatch::PauseOnEnqueueSemaphoreEnd:
            case CommandToPatch::PauseOnEnqueuePipeControlStart:
            case CommandToPatch::PauseOnEnqueuePipeControlEnd:
                UNRECOVERABLE_IF(commandToPatch.pCommand == nullptr);
                break;
            case CommandToPatch::ComputeWalkerInlineDataScratch:
            case CommandToPatch::ComputeWalkerImplicitArgsScratch:
            case CommandToPatch::NoopSpace:
                break;
            default:
                UNRECOVERABLE_IF(true);
            }
        }
        commandsToPatch.clear();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::getTotalSizeForCopyRegion(const ze_copy_region_t *region, uint32_t pitch, uint32_t slicePitch) {
    if (region->depth > 1) {
        uint32_t offset = region->originX + ((region->originY) * pitch) + ((region->originZ) * slicePitch);
        return (region->width * region->height * region->depth) + offset;
    } else {
        uint32_t offset = region->originX + ((region->originY) * pitch);
        return (region->width * region->height) + offset;
    }
}

inline NEO::MemoryPool getMemoryPoolFromAllocDataForSplit(bool allocFound, const NEO::SvmAllocationData *allocData) {
    if (allocFound) {
        return allocData->gpuAllocations.getDefaultGraphicsAllocation()->getMemoryPool();
    } else if (NEO::debugManager.flags.SplitBcsCopyHostptr.get() != 0) {
        return NEO::MemoryPool::system4KBPages;
    }
    return NEO::MemoryPool::memoryNull;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isAppendSplitNeeded(void *dstPtr, const void *srcPtr, size_t size, NEO::TransferDirection &directionOut) {
    if (size < minimalSizeForBcsSplit) {
        return false;
    }

    NEO::SvmAllocationData *srcAllocData = nullptr;
    NEO::SvmAllocationData *dstAllocData = nullptr;
    bool srcAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(const_cast<void *>(srcPtr), size, srcAllocData);
    bool dstAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(dstPtr, size, dstAllocData);

    auto srcMemoryPool = getMemoryPoolFromAllocDataForSplit(srcAllocFound, srcAllocData);
    auto dstMemoryPool = getMemoryPoolFromAllocDataForSplit(dstAllocFound, dstAllocData);
    for (const auto memoryPool : {srcMemoryPool, dstMemoryPool}) {
        if (memoryPool == NEO::MemoryPool::memoryNull) {
            return false;
        }
    }

    return this->isAppendSplitNeeded(dstMemoryPool, srcMemoryPool, size, directionOut);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline bool CommandListCoreFamily<gfxCoreFamily>::isAppendSplitNeeded(NEO::MemoryPool dstPool, NEO::MemoryPool srcPool, size_t size, NEO::TransferDirection &directionOut) {
    directionOut = NEO::createTransferDirection(!NEO::MemoryPoolHelper::isSystemMemoryPool(srcPool), !NEO::MemoryPoolHelper::isSystemMemoryPool(dstPool));

    return this->isBcsSplitNeeded &&
           size >= minimalSizeForBcsSplit &&
           directionOut != NEO::TransferDirection::localToLocal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programStateBaseAddress(NEO::CommandContainer &container, bool useSbaProperties) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    bool isRcs = (this->engineGroupType == NEO::EngineGroupType::renderCompute);

    uint32_t statelessMocsIndex = this->defaultMocsIndex;
    NEO::StateBaseAddressProperties *sbaProperties = useSbaProperties ? &this->finalStreamState.stateBaseAddress : nullptr;

    STATE_BASE_ADDRESS sba;

    NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(*commandContainer.getCommandStream(), this->device->getNEODevice()->getRootDeviceEnvironment(), isRcs, this->dcFlushSupport);

    NEO::EncodeStateBaseAddressArgs<GfxFamily> encodeStateBaseAddressArgs = {
        &commandContainer,                        // container
        sba,                                      // sbaCmd
        sbaProperties,                            // sbaProperties
        statelessMocsIndex,                       // statelessMocsIndex
        l1CachePolicyData.getL1CacheValue(false), // l1CachePolicy
        l1CachePolicyData.getL1CacheValue(true),  // l1CachePolicyDebuggerActive
        this->partitionCount > 1,                 // multiOsContextCapable
        isRcs,                                    // isRcs
        this->doubleSbaWa,                        // doubleSbaWa
        this->heaplessModeEnabled                 // heaplessModeEnabled
    };
    auto offsetSbaCmd = container.getCommandStream()->getUsed();
    NEO::EncodeStateBaseAddress<GfxFamily>::encode(encodeStateBaseAddressArgs);

    bool sbaTrackingEnabled = NEO::Debugger::isDebugEnabled(this->internalUsage) && this->device->getL0Debugger();
    NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                 *this->device->getNEODevice(),
                                                                                 *container.getCommandStream(),
                                                                                 sba, (isImmediateType() || this->dispatchCmdListBatchBufferAsPrimary));

    programStateBaseAddressHook(offsetSbaCmd, sba.getSurfaceStateBaseAddressModifyEnable());
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isSkippingInOrderBarrierAllowed(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) const {
    uint32_t eventsToWait = numWaitEvents;

    for (uint32_t i = 0; i < numWaitEvents; i++) {
        if (CommandListCoreFamily<gfxCoreFamily>::canSkipInOrderEventWait(*Event::fromHandle(phWaitEvents[i]), false)) {
            eventsToWait--;
        }
    }

    if (eventsToWait > 0) {
        return false;
    }

    auto signalEvent = Event::fromHandle(hSignalEvent);

    return !(signalEvent && (signalEvent->isEventTimestampFlagSet() || !signalEvent->isCounterBased()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {
    if (isInOrderExecutionEnabled() && isSkippingInOrderBarrierAllowed(hSignalEvent, numWaitEvents, phWaitEvents)) {
        if (hSignalEvent) {
            assignInOrderExecInfoToEvent(Event::fromHandle(hSignalEvent));
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, relaxedOrderingDispatch, true, true, false, false);
    if (ret) {
        return ret;
    }

    appendSynchronizedDispatchInitializationSection();

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(signalEvent, nullptr, true, false, false, isCopyOnly(false));

    if (!this->isInOrderExecutionEnabled()) {
        if (isCopyOnly(false)) {
            NEO::MiFlushArgs args{this->dummyBlitWa};
            uint64_t gpuAddress = 0u;
            TaskCountType value = 0u;
            if (isImmediateType()) {
                args.commandWithPostSync = true;
                auto csr = getCsr(false);
                gpuAddress = csr->getBarrierCountGpuAddress();
                value = csr->getNextBarrierCount() + 1;
                commandContainer.addToResidencyContainer(csr->getTagAllocation());
            }

            encodeMiFlush(gpuAddress, value, args);
        } else {
            appendComputeBarrierCommand();
        }
    }

    addToMappedEventList(signalEvent);
    appendSignalEventPostWalker(signalEvent, nullptr, nullptr, this->isInOrderExecutionEnabled(), false, isCopyOnly(false));

    if (isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
    }
    handleInOrderDependencyCounter(signalEvent, false, false);

    appendSynchronizedDispatchCleanupSection();

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::setupFillKernelArguments(size_t baseOffset,
                                                                    size_t patternSize,
                                                                    size_t dstSize,
                                                                    CmdListFillKernelArguments &outArguments,
                                                                    Kernel *kernel) {
    const auto maxWgSize = this->device->getDeviceInfo().maxWorkGroupSize;
    if (canUseImmediateFill(dstSize, patternSize, baseOffset, maxWgSize)) {
        constexpr auto dataTypeSize = sizeof(uint32_t) * 4;

        size_t middleSize = dstSize;
        outArguments.mainOffset = baseOffset;
        outArguments.leftRemainingBytes = sizeof(uint32_t) - (baseOffset % sizeof(uint32_t));
        if (baseOffset % sizeof(uint32_t) != 0 && outArguments.leftRemainingBytes <= dstSize) {
            middleSize -= outArguments.leftRemainingBytes;
            outArguments.mainOffset += outArguments.leftRemainingBytes;
        } else {
            outArguments.leftRemainingBytes = 0;
        }

        size_t adjustedSize = middleSize / dataTypeSize;
        outArguments.mainGroupSize = maxWgSize;
        if (outArguments.mainGroupSize > adjustedSize && adjustedSize > 0) {
            outArguments.mainGroupSize = adjustedSize;
        }

        outArguments.groups = adjustedSize / outArguments.mainGroupSize;
        outArguments.rightRemainingBytes = static_cast<uint32_t>((adjustedSize % outArguments.mainGroupSize) * dataTypeSize +
                                                                 middleSize % dataTypeSize);

        if (outArguments.rightRemainingBytes > 0) {
            outArguments.rightOffset = outArguments.mainOffset + (middleSize - outArguments.rightRemainingBytes);
        }
    } else {
        size_t elSize = sizeof(uint32_t);
        if (baseOffset % elSize != 0) {
            outArguments.leftRemainingBytes = static_cast<uint32_t>(elSize - (baseOffset % elSize));
        }
        if (outArguments.leftRemainingBytes > 0) {
            elSize = sizeof(uint8_t);
        }
        size_t adjustedSize = dstSize / elSize;
        uint32_t groupSizeX = static_cast<uint32_t>(adjustedSize);
        uint32_t groupSizeY = 1, groupSizeZ = 1;
        kernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
        outArguments.mainGroupSize = groupSizeX;

        outArguments.groups = static_cast<uint32_t>(adjustedSize) / outArguments.mainGroupSize;
        outArguments.rightRemainingBytes = static_cast<uint32_t>((adjustedSize % outArguments.mainGroupSize) * elSize +
                                                                 dstSize % elSize);

        size_t patternAllocationSize = alignUp(patternSize, MemoryConstants::cacheLineSize);
        outArguments.patternSizeInEls = static_cast<uint32_t>(patternAllocationSize / elSize);

        if (outArguments.rightRemainingBytes > 0) {
            outArguments.rightOffset = outArguments.groups * outArguments.mainGroupSize * elSize;
            outArguments.patternOffsetRemainder = (outArguments.mainGroupSize * outArguments.groups & (outArguments.patternSizeInEls - 1)) * elSize;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitOnMemory(void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    auto descriptor = reinterpret_cast<zex_wait_on_mem_desc_t *>(desc);
    COMPARE_OPERATION comparator;
    switch (descriptor->actionFlag) {
    case ZEX_WAIT_ON_MEMORY_FLAG_EQUAL:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD;
        break;
    case ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD;
        break;
    case ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_SDD;
        break;
    case ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD;
        break;
    case ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_SDD;
        break;
    case ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL:
        comparator = COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    Event *signalEvent = nullptr;
    if (signalEventHandle) {
        signalEvent = Event::fromHandle(signalEventHandle);
    }

    auto srcAllocationStruct = getAlignedAllocationData(this->device, false, ptr, sizeof(uint32_t), true, false);
    if (srcAllocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    UNRECOVERABLE_IF(srcAllocationStruct.alloc == nullptr);

    if (this->isInOrderExecutionEnabled()) {
        handleInOrderImplicitDependencies(false, false);
    }

    if (!handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    appendEventForProfiling(signalEvent, nullptr, true, false, false, isCopyOnly(false));

    commandContainer.addToResidencyContainer(srcAllocationStruct.alloc);
    uint64_t gpuAddress = static_cast<uint64_t>(srcAllocationStruct.alignedAllocationPtr);

    bool indirectMode = false;

    if (useQwordData) {
        if (isQwordInOrderCounter()) {
            indirectMode = true;

            NEO::LriHelper<GfxFamily>::program(commandContainer.getCommandStream(), RegisterOffsets::csGprR0, getLowPart(data), true, isCopyOnly(false));
            NEO::LriHelper<GfxFamily>::program(commandContainer.getCommandStream(), RegisterOffsets::csGprR0 + 4, getHighPart(data), true, isCopyOnly(false));

        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    } else {
        UNRECOVERABLE_IF(getHighPart(data) != 0);
    }

    NEO::EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*commandContainer.getCommandStream(), gpuAddress, data, comparator, false, useQwordData, indirectMode, false, nullptr);

    const auto &rootDeviceEnvironment = this->device->getNEODevice()->getRootDeviceEnvironment();
    auto allocType = srcAllocationStruct.alloc->getAllocationType();
    bool isSystemMemoryUsed =
        (allocType == NEO::AllocationType::bufferHostMemory) ||
        (allocType == NEO::AllocationType::externalHostPtr);
    if (isSystemMemoryUsed) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(*commandContainer.getCommandStream(), gpuAddress, NEO::FenceType::acquire, rootDeviceEnvironment);
    }

    appendSignalEventPostWalker(signalEvent, nullptr, nullptr, false, false, isCopyOnly(false));

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(signalEvent, false, false, false);
    }
    handleInOrderDependencyCounter(signalEvent, false, false);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWriteToMemory(void *desc,
                                                                      void *ptr,
                                                                      uint64_t data) {
    auto descriptor = reinterpret_cast<zex_write_to_mem_desc_t *>(desc);

    size_t bufSize = sizeof(uint64_t);
    auto dstAllocationStruct = getAlignedAllocationData(this->device, false, ptr, bufSize, false, false);
    if (dstAllocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    UNRECOVERABLE_IF(dstAllocationStruct.alloc == nullptr);
    commandContainer.addToResidencyContainer(dstAllocationStruct.alloc);

    if (this->isInOrderExecutionEnabled()) {
        handleInOrderImplicitDependencies(false, false);
    }

    const uint64_t gpuAddress = static_cast<uint64_t>(dstAllocationStruct.alignedAllocationPtr);

    if (isCopyOnly(false)) {
        NEO::MiFlushArgs args{this->dummyBlitWa};
        args.commandWithPostSync = true;
        encodeMiFlush(gpuAddress,
                      data, args);
    } else {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = getDcFlushRequired(!!descriptor->writeScope);
        args.dcFlushEnable &= dstAllocationStruct.needsFlush;

        NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *commandContainer.getCommandStream(),
            NEO::PostSyncMode::immediateData,
            gpuAddress,
            data,
            device->getNEODevice()->getRootDeviceEnvironment(),
            args);
    }

    if (this->isInOrderExecutionEnabled()) {
        appendSignalInOrderDependencyCounter(nullptr, false, false, false);
    }
    handleInOrderDependencyCounter(nullptr, false, false);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitExternalSemaphores(uint32_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                                               const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                                               uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSignalExternalSemaphores(size_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                                                 const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::allocateOrReuseKernelPrivateMemoryIfNeeded(Kernel *kernel, uint32_t sizePerHwThread) {
    L0::KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    if (sizePerHwThread != 0U && kernelImp->getParentModule().shouldAllocatePrivateMemoryPerDispatch()) {
        allocateOrReuseKernelPrivateMemory(kernel, sizePerHwThread, ownedPrivateAllocations);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::allocateOrReuseKernelPrivateMemory(Kernel *kernel, uint32_t sizePerHwThread, NEO::PrivateAllocsToReuseContainer &privateAllocsToReuse) {
    L0::KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    NEO::GraphicsAllocation *privateAlloc = nullptr;

    bool allocToReuseFound = false;

    for (auto &alloc : privateAllocsToReuse) {
        if (sizePerHwThread == alloc.first) {
            privateAlloc = alloc.second;
            allocToReuseFound = true;
            break;
        }
    }
    if (!allocToReuseFound) {
        privateAlloc = kernelImp->allocatePrivateMemoryGraphicsAllocation();
        privateAllocsToReuse.push_back({sizePerHwThread, privateAlloc});
        this->commandContainer.addToResidencyContainer(privateAlloc);
    }
    kernel->patchCrossthreadDataWithPrivateAllocation(privateAlloc);
}

template <GFXCORE_FAMILY gfxCoreFamily>
CmdListEventOperation CommandListCoreFamily<gfxCoreFamily>::estimateEventPostSync(Event *event, uint32_t operations) {
    CmdListEventOperation ret;

    UNRECOVERABLE_IF(operations & (this->partitionCount - 1));

    ret.operationCount = operations / this->partitionCount;
    ret.operationOffset = event->getSinglePacketSize() * this->partitionCount;
    ret.workPartitionOperation = this->partitionCount > 1;
    ret.isTimestmapEvent = event->isEventTimestampFlagSet();
    ret.completionFieldOffset = event->getCompletionFieldOffset();

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCopy(uint64_t gpuAddress, uint32_t value, bool workloadPartition, void **outCmdBuffer) {

    NEO::MiFlushArgs miFlushArgs{this->dummyBlitWa};
    miFlushArgs.commandWithPostSync = true;

    encodeMiFlush(gpuAddress, value, miFlushArgs);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCompute(uint64_t gpuAddress, uint32_t value, bool workloadPartition, void **outCmdBuffer) {
    NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(
        *commandContainer.getCommandStream(),
        gpuAddress,
        value,
        0u,
        false,
        workloadPartition,
        outCmdBuffer);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCommands(const CmdListEventOperation &eventOperations, uint64_t gpuAddress, void **syncCmdBuffer, CommandToPatchContainer *outListCommands,
                                                                    uint32_t value, bool useLastPipeControl, bool signalScope, bool skipPartitionOffsetProgramming, bool copyOperation) {
    decltype(&CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCompute) dispatchFunction = &CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCompute;
    if (copyOperation) {
        dispatchFunction = &CommandListCoreFamily<gfxCoreFamily>::dispatchPostSyncCopy;
    }

    auto operationCount = eventOperations.operationCount;
    if (useLastPipeControl) {
        operationCount--;
    }

    if (eventOperations.isTimestmapEvent && !skipPartitionOffsetProgramming) {
        appendDispatchOffsetRegister(eventOperations.workPartitionOperation, true);
    }

    void **outCmdBuffer = nullptr;
    void *outCmd = nullptr;
    if (outListCommands != nullptr) {
        outCmdBuffer = &outCmd;
    }

    for (uint32_t i = 0; i < operationCount; i++) {
        (this->*dispatchFunction)(gpuAddress, value, eventOperations.workPartitionOperation, outCmdBuffer);
        if (outListCommands != nullptr) {
            auto &cmdToPatch = outListCommands->emplace_back();
            cmdToPatch.type = CommandToPatch::CbEventTimestampClearStoreDataImm;
            cmdToPatch.offset = i * eventOperations.operationOffset + eventOperations.completionFieldOffset;
            cmdToPatch.pDestination = outCmd;
        }

        gpuAddress += eventOperations.operationOffset;
    }

    if (useLastPipeControl) {
        NEO::PipeControlArgs pipeControlArgs;
        pipeControlArgs.dcFlushEnable = getDcFlushRequired(signalScope);
        pipeControlArgs.workloadPartitionOffset = eventOperations.workPartitionOperation;

        const auto &productHelper = this->device->getNEODevice()->getRootDeviceEnvironment().template getHelper<NEO::ProductHelper>();
        if (productHelper.isDirectSubmissionConstantCacheInvalidationNeeded(this->device->getHwInfo())) {
            if (isImmediateType()) {
                pipeControlArgs.constantCacheInvalidationEnable = getCsr(false)->isDirectSubmissionEnabled();
            } else {
                pipeControlArgs.constantCacheInvalidationEnable = this->device->getNEODevice()->isAnyDirectSubmissionEnabled();
            }
        }

        NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *commandContainer.getCommandStream(),
            NEO::PostSyncMode::immediateData,
            gpuAddress,
            value,
            device->getNEODevice()->getRootDeviceEnvironment(),
            pipeControlArgs);

        if (syncCmdBuffer != nullptr) {
            *syncCmdBuffer = pipeControlArgs.postSyncCmd;
        }
    }

    if (eventOperations.isTimestmapEvent && !skipPartitionOffsetProgramming) {
        appendDispatchOffsetRegister(eventOperations.workPartitionOperation, false);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchEventPostSyncOperation(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outListCommands, uint32_t value, bool omitFirstOperation, bool useMax,
                                                                          bool useLastPipeControl, bool skipPartitionOffsetProgramming, bool copyOeration) {
    if (!event->getAllocation(this->device)) {
        return;
    }

    uint32_t packets = event->getPacketsInUse();
    if (this->signalAllEventPackets || useMax) {
        packets = event->getMaxPacketsCount();
    }
    auto eventPostSync = estimateEventPostSync(event, packets);

    uint64_t gpuAddress = event->getCompletionFieldGpuAddress(this->device);
    if (omitFirstOperation) {
        gpuAddress += eventPostSync.operationOffset;
        eventPostSync.operationCount--;
    }

    dispatchPostSyncCommands(eventPostSync, gpuAddress, syncCmdBuffer, outListCommands, value, useLastPipeControl, event->isSignalScope(), skipPartitionOffsetProgramming, copyOeration);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchEventRemainingPacketsPostSyncOperation(Event *event, bool copyOperation) {
    if (this->signalAllEventPackets && !event->isCounterBasedExplicitlyEnabled() && event->getPacketsInUse() < event->getMaxPacketsCount()) {
        uint32_t packets = event->getMaxPacketsCount() - event->getPacketsInUse();
        CmdListEventOperation remainingPacketsOperation = estimateEventPostSync(event, packets);

        uint64_t eventAddress = event->getCompletionFieldGpuAddress(device);
        eventAddress += event->getSinglePacketSize() * event->getPacketsInUse();

        constexpr bool appendLastPipeControl = false;
        dispatchPostSyncCommands(remainingPacketsOperation, eventAddress, nullptr, nullptr, Event::STATE_SIGNALED, appendLastPipeControl, event->isSignalScope(), false, copyOperation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendWaitOnSingleEvent(Event *event, CommandToPatchContainer *outWaitCmds, bool relaxedOrderingAllowed, bool dualStreamCopyOffload, CommandToPatch::CommandType storedSemaphore) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    uint64_t gpuAddr = event->getCompletionFieldGpuAddress(this->device);
    uint32_t packetsToWait = event->getPacketsToWait();

    void **outSemWaitCmdBuffer = nullptr;
    void *outSemWaitCmd = nullptr;
    if (outWaitCmds != nullptr) {
        outSemWaitCmdBuffer = &outSemWaitCmd;
    }

    for (uint32_t i = 0u; i < packetsToWait; i++) {
        if (relaxedOrderingAllowed) {
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(*commandContainer.getCommandStream(), 0, gpuAddr, Event::STATE_CLEARED,
                                                                                                   NEO::CompareOperation::equal, true, false, isCopyOnly(dualStreamCopyOffload));
        } else {
            NEO::EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*commandContainer.getCommandStream(),
                                                                       gpuAddr,
                                                                       Event::STATE_CLEARED,
                                                                       COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, false, false, false, false, outSemWaitCmdBuffer);

            if (outWaitCmds != nullptr) {
                auto &semWaitCmd = outWaitCmds->emplace_back();
                semWaitCmd.type = storedSemaphore;
                semWaitCmd.offset = i * event->getSinglePacketSize() + event->getCompletionFieldOffset();
                semWaitCmd.pDestination = outSemWaitCmd;
            }
        }

        gpuAddr += event->getSinglePacketSize();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::addCmdForPatching(std::shared_ptr<NEO::InOrderExecInfo> *externalInOrderExecInfo, void *cmd1, void *cmd2, uint64_t counterValue, NEO::InOrderPatchCommandHelpers::PatchCmdType patchCmdType) {
    if ((NEO::debugManager.flags.EnableInOrderRegularCmdListPatching.get() != 0) && !isImmediateType()) {
        this->inOrderPatchCmds.emplace_back(externalInOrderExecInfo, cmd1, cmd2, counterValue, patchCmdType, this->inOrderAtomicSignalingEnabled, this->duplicatedInOrderCounterStorageEnabled);
        return this->inOrderPatchCmds.size() - 1;
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::patchInOrderCmds() {
    if (inOrderPatchCmds.size() == 0) {
        return;
    }

    uint64_t implicitAppendCounter = 0;
    bool hasRgularCmdListSubmissionCounter = false;

    if (isInOrderExecutionEnabled()) {
        implicitAppendCounter = NEO::InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo);
        hasRgularCmdListSubmissionCounter = (inOrderExecInfo->getRegularCmdListSubmissionCounter() > 1);
    }

    for (auto &cmd : inOrderPatchCmds) {
        if (cmd.isExternalDependency() || hasRgularCmdListSubmissionCounter) {
            cmd.patch(implicitAppendCounter);
        }
    }
}
template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::hasInOrderDependencies() const {
    return (inOrderExecInfo.get() && inOrderExecInfo->getCounterValue() > 0);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::handleCounterBasedEventOperations(Event *signalEvent, bool skipAddingEventToResidency) {
    if (!signalEvent) {
        return true;
    }

    bool implicitCounterBasedEventConversionEnable = !this->dcFlushSupport;
    if (NEO::debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.get() != -1) {
        implicitCounterBasedEventConversionEnable = !!NEO::debugManager.flags.EnableImplicitConvertionToCounterBasedEvents.get();
    }

    if (implicitCounterBasedEventConversionEnable && !signalEvent->isCounterBasedExplicitlyEnabled()) {
        if (isInOrderExecutionEnabled() && isImmediateType()) {
            signalEvent->enableCounterBasedMode(false, ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE);
        } else {
            signalEvent->disableImplicitCounterBasedMode();
        }
    }

    if (signalEvent->isCounterBased()) {
        if (!isInOrderExecutionEnabled() || signalEvent->isIpcImported()) {
            return false;
        }

        const auto counterBasedFlags = signalEvent->getCounterBasedFlags();

        if (isImmediateType() && !(counterBasedFlags & ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE)) {
            return false;
        }

        if (!isImmediateType()) {
            if (!(counterBasedFlags & ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE)) {
                return false;
            }

            if (signalEvent->isKmdWaitModeEnabled()) {
                this->interruptEvents.push_back(signalEvent);
            }
        }

        if (signalEvent->isUsingContextEndOffset()) {
            auto tag = device->getInOrderTimestampAllocator()->getTag();

            if (skipAddingEventToResidency == false) {
                this->commandContainer.addToResidencyContainer(tag->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex()));
            }
            signalEvent->resetInOrderTimestampNode(tag, this->partitionCount);
            signalEvent->resetAdditionalTimestampNode(nullptr, this->partitionCount, false);
        }
    }

    return true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint64_t CommandListCoreFamily<gfxCoreFamily>::getInOrderIncrementValue() const {
    return (this->inOrderAtomicSignalingEnabled ? this->getPartitionCount() : 1);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::encodeMiFlush(uint64_t immediateDataGpuAddress, uint64_t immediateData, NEO::MiFlushArgs &args) {
    args.waArgs.isWaRequired &= args.commandWithPostSync;
    auto isDummyBlitRequired = NEO::BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(args.waArgs);
    NEO::EncodeMiFlushDW<GfxFamily>::programWithWa(*commandContainer.getCommandStream(), immediateDataGpuAddress, immediateData, args);
    if (isDummyBlitRequired) {
        const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
        auto dummyAllocation = rootDeviceEnvironment.getDummyAllocation();
        commandContainer.addToResidencyContainer(dummyAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::updateInOrderExecInfo(size_t inOrderPatchIndex, std::shared_ptr<NEO::InOrderExecInfo> *inOrderExecInfo, bool disablePatchingFlag) {
    auto &patchCmd = inOrderPatchCmds[inOrderPatchIndex];
    patchCmd.updateInOrderExecInfo(inOrderExecInfo);
    patchCmd.setSkipPatching(disablePatchingFlag);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandListCoreFamily<gfxCoreFamily>::disablePatching(size_t inOrderPatchIndex) {
    auto &patchCmd = inOrderPatchCmds[inOrderPatchIndex];
    patchCmd.setSkipPatching(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandListCoreFamily<gfxCoreFamily>::enablePatching(size_t inOrderPatchIndex) {
    auto &patchCmd = inOrderPatchCmds[inOrderPatchIndex];
    patchCmd.setSkipPatching(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline bool CommandListCoreFamily<gfxCoreFamily>::isCbEventBoundToCmdList(Event *event) const {
    return event->isCounterBased() && event->getInOrderExecInfo().get() == inOrderExecInfo.get();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSynchronizedDispatchInitializationSection() {
    auto syncAlloc = device->getSyncDispatchTokenAllocation();

    if (getSynchronizedDispatchMode() != NEO::SynchronizedDispatchMode::disabled) {
        commandContainer.addToResidencyContainer(syncAlloc);
    }

    if (getSynchronizedDispatchMode() == NEO::SynchronizedDispatchMode::limited) {
        NEO::EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*commandContainer.getCommandStream(), syncAlloc->getGpuAddress() + sizeof(uint32_t), 0u,
                                                                   GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                   false, false, false, true, nullptr);
    } else if (getSynchronizedDispatchMode() == NEO::SynchronizedDispatchMode::full) {
        appendFullSynchronizedDispatchInit();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendFullSynchronizedDispatchInit() {
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using ATOMIC_OPCODES = typename MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename MI_ATOMIC::DATA_SIZE;

    constexpr size_t conditionalDataMemBbStartSize = NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataMemBatchBufferStart(false);

    const uint32_t queueId = this->syncDispatchQueueId + 1;
    const uint64_t queueIdToken = static_cast<uint64_t>(queueId) << 32;
    const uint64_t tokenInitialValue = queueIdToken + this->partitionCount;

    auto syncAllocationGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();
    auto workPartitionAllocationGpuVa = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocation()->getGpuAddress();
    auto cmdStream = commandContainer.getCommandStream();

    // If Secondary Tile, then jump to Secondary Tile section
    // Reserve space for now. Will be patched later
    NEO::LinearStream skipPrimaryTileSectionCmdStream(cmdStream->getSpace(conditionalDataMemBbStartSize), conditionalDataMemBbStartSize);

    // If token acquired, jump to the end
    NEO::LinearStream jumpToEndSectionFromPrimaryTile;

    // Primary Tile section
    {
        // Try acquire token
        uint64_t acquireTokenCmdBufferVa = cmdStream->getCurrentGpuAddressPosition();
        NEO::EncodeMiPredicate<GfxFamily>::encode(*cmdStream, NEO::MiPredicateType::disable);
        NEO::EncodeAtomic<GfxFamily>::programMiAtomic(*cmdStream, syncAllocationGpuVa, ATOMIC_OPCODES::ATOMIC_8B_CMP_WR,
                                                      DATA_SIZE::DATA_SIZE_QWORD, 1, 1, 0, tokenInitialValue);

        // If token acquired, jump to the end
        // Reserve space for now. Will be patched later
        jumpToEndSectionFromPrimaryTile.replaceBuffer(cmdStream->getSpace(conditionalDataMemBbStartSize), conditionalDataMemBbStartSize);

        // Semaphore for potential switch
        NEO::EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*cmdStream, syncAllocationGpuVa + sizeof(uint32_t), 0u,
                                                                   GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                   false, false, false, true, nullptr);

        // Loop back to acquire again
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(cmdStream, acquireTokenCmdBufferVa, false, false, false);
    }

    // Patch Primary Tile section skip (to Secondary Tile section)
    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(skipPrimaryTileSectionCmdStream, cmdStream->getCurrentGpuAddressPosition(), workPartitionAllocationGpuVa, 0,
                                                                                           NEO::CompareOperation::notEqual, false, false, isCopyOnly(false));

    // Secondary Tile section
    {
        NEO::EncodeMiPredicate<GfxFamily>::encode(*cmdStream, NEO::MiPredicateType::disable);

        // Wait for token acquisition by Primary Tile
        NEO::EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*cmdStream, syncAllocationGpuVa + sizeof(uint32_t), queueId,
                                                                   GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                                   false, false, false, true, nullptr);
    }

    // Patch Primary Tile section jump to end
    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(jumpToEndSectionFromPrimaryTile, cmdStream->getCurrentGpuAddressPosition(), syncAllocationGpuVa + sizeof(uint32_t), queueId,
                                                                                           NEO::CompareOperation::equal, false, false, isCopyOnly(false));

    // End section
    NEO::EncodeMiPredicate<GfxFamily>::encode(*cmdStream, NEO::MiPredicateType::disable);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::addPatchScratchAddressInImplicitArgs(CommandsToPatch &commandsToPatch, NEO::EncodeDispatchKernelArgs &args, const NEO::KernelDescriptor &kernelDescriptor, bool kernelNeedsImplicitArgs) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSynchronizedDispatchCleanupSection() {
    if (getSynchronizedDispatchMode() != NEO::SynchronizedDispatchMode::full) {
        return;
    }

    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using ATOMIC_OPCODES = typename MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename MI_ATOMIC::DATA_SIZE;

    auto cmdStream = commandContainer.getCommandStream();
    auto syncAllocationGpuVa = device->getSyncDispatchTokenAllocation()->getGpuAddress();

    const uint64_t queueIdToken = static_cast<uint64_t>(this->syncDispatchQueueId + 1) << 32;

    NEO::EncodeAtomic<GfxFamily>::programMiAtomic(*cmdStream, syncAllocationGpuVa, ATOMIC_OPCODES::ATOMIC_8B_DECREMENT, DATA_SIZE::DATA_SIZE_QWORD, 1, 1, 0, 0);

    NEO::EncodeAtomic<GfxFamily>::programMiAtomic(*cmdStream, syncAllocationGpuVa, ATOMIC_OPCODES::ATOMIC_8B_CMP_WR, DATA_SIZE::DATA_SIZE_QWORD, 1, 1, queueIdToken, 0);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isDeviceToHostCopyEventFenceRequired(Event *signalEvent) const {
    return (signalEvent && signalEvent->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST)) || (isImmediateType() && isInOrderExecutionEnabled());
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isDeviceToHostBcsCopy(NEO::GraphicsAllocation *srcAllocation, NEO::GraphicsAllocation *dstAllocation, bool copyEngineOperation) const {
    bool srcInLocalPool = false;
    bool dstInLocalPool = false;
    if (srcAllocation) {
        srcInLocalPool = srcAllocation->isAllocatedInLocalMemoryPool();
    }
    if (dstAllocation) {
        dstInLocalPool = dstAllocation->isAllocatedInLocalMemoryPool();
    }
    return (copyEngineOperation && (srcInLocalPool && !dstInLocalPool));
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendCopyOperationFence(Event *signalEvent, NEO::GraphicsAllocation *srcAllocation, NEO::GraphicsAllocation *dstAllocation, bool copyEngineOperation) {
    if (this->copyOperationFenceSupported && isDeviceToHostBcsCopy(srcAllocation, dstAllocation, copyEngineOperation)) {
        if (isDeviceToHostCopyEventFenceRequired(signalEvent)) {
            NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(*commandContainer.getCommandStream(), 0, NEO::FenceType::release, device->getNEODevice()->getRootDeviceEnvironment());
            taskCountUpdateFenceRequired = false;
        } else {
            taskCountUpdateFenceRequired = true;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                                                     ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfilingAllWalkers(Event *event, void **syncCmdBuffer, CommandToPatchContainer *outTimeStampSyncCmds, bool beforeWalker, bool singlePacketEvent, bool skipAddingEventToResidency, bool copyOperation) {
    if (copyOperation || singleEventPacketRequired(singlePacketEvent)) {
        if (beforeWalker) {
            appendEventForProfiling(event, outTimeStampSyncCmds, true, false, skipAddingEventToResidency, copyOperation);
        } else {
            appendSignalEventPostWalker(event, syncCmdBuffer, outTimeStampSyncCmds, false, skipAddingEventToResidency, copyOperation);
        }
    } else {
        if (event) {
            if (beforeWalker) {
                event->resetKernelCountAndPacketUsedCount();
                event->zeroKernelCount();
            } else {
                if (event->getKernelCount() > 1) {
                    if (getDcFlushRequired(event->isSignalScope())) {
                        programEventL3Flush(event);
                    }
                    dispatchEventRemainingPacketsPostSyncOperation(event, copyOperation);
                }
            }
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programEventL3Flush(Event *event) {
    auto eventPartitionOffset = (partitionCount > 1) ? (partitionCount * event->getSinglePacketSize())
                                                     : event->getSinglePacketSize();
    uint64_t eventAddress = event->getPacketAddress(device) + eventPartitionOffset;
    if (event->isUsingContextEndOffset()) {
        eventAddress += event->getContextEndOffset();
    }

    if (partitionCount > 1) {
        event->setPacketsInUse(event->getPacketsUsedInLastKernel() + partitionCount);
    } else {
        event->setPacketsInUse(event->getPacketsUsedInLastKernel() + 1);
    }

    event->setL3FlushForCurrentKernel();

    auto &cmdListStream = *commandContainer.getCommandStream();
    NEO::PipeControlArgs args;
    args.dcFlushEnable = true;
    args.workloadPartitionOffset = partitionCount > 1;

    NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        cmdListStream,
        NEO::PostSyncMode::immediateData,
        eventAddress,
        Event::STATE_SIGNALED,
        device->getNEODevice()->getRootDeviceEnvironment(),
        args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::setAdditionalKernelLaunchParams(CmdListKernelLaunchParams &launchParams, Kernel &kernel) const {
    auto &kernelDescriptor = kernel.getImmutableData()->getDescriptor();

    if (launchParams.localRegionSize == NEO::localRegionSizeParamNotSet) {
        launchParams.localRegionSize = kernelDescriptor.kernelAttributes.localRegionSize;
    }
    if (launchParams.requiredDispatchWalkOrder == NEO::RequiredDispatchWalkOrder::none) {
        launchParams.requiredDispatchWalkOrder = kernelDescriptor.kernelAttributes.dispatchWalkOrder;
    }
    if (launchParams.requiredPartitionDim == NEO::RequiredPartitionDim::none) {
        launchParams.requiredPartitionDim = kernelDescriptor.kernelAttributes.partitionDim;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::dispatchInOrderPostOperationBarrier(Event *signalEvent, bool dcFlushRequired, bool copyOperation) {
    if ((!signalEvent || !signalEvent->getAllocation(this->device)) && !copyOperation) {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = dcFlushRequired;

        NEO::MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(args);
        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isAllocationImported(NEO::GraphicsAllocation *gpuAllocation, NEO::SVMAllocsManager *svmManager) const {

    if (svmManager) {
        NEO::SvmAllocationData *allocData = svmManager->getSVMAlloc(reinterpret_cast<void *>(gpuAllocation->getGpuAddress()));
        if (allocData && allocData->isImportedAllocation) {
            return true;
        }
    }

    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isKernelUncachedMocsRequired(bool kernelState) {
    this->containsStatelessUncachedResource |= kernelState;
    auto &productHelper = this->device->getNEODevice()->getProductHelper();
    auto &rootDeviceEnvironment = this->device->getNEODevice()->getRootDeviceEnvironment();

    if (this->stateBaseAddressTracking || productHelper.deferMOCSToPatIndex(rootDeviceEnvironment.isWddmOnLinux())) {
        return false;
    }
    return this->containsStatelessUncachedResource;
}

} // namespace L0
