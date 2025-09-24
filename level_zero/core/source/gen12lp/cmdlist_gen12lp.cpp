/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/program/kernel_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/gen12lp/definitions/cache_flush_gen12lp.inl"

#include "cmdlist_extended.inl"
#include "implicit_args.h"

namespace L0 {

constexpr auto gfxCoreFamily = IGFX_GEN12LP_CORE;
template <>
inline NEO::PreemptionMode CommandListCoreFamily<gfxCoreFamily>::obtainKernelPreemptionMode(Kernel *kernel) {
    NEO::PreemptionFlags flags = NEO::PreemptionHelper::createPreemptionLevelFlags(*device->getNEODevice(), &kernel->getImmutableData()->getDescriptor());
    return NEO::PreemptionHelper::taskPreemptionMode(device->getDevicePreemptionMode(), flags);
}

template <>
inline NEO::PipeControlArgs CommandListCoreFamily<gfxCoreFamily>::createBarrierFlags() {
    NEO::PipeControlArgs args;
    args.isWalkerWithProfilingEnqueued = this->getAndClearIsWalkerWithProfilingEnqueued();
    return args;
}

template <>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiTileBarrier(NEO::Device &neoDevice) {
}

template <>
size_t CommandListCoreFamily<gfxCoreFamily>::estimateBufferSizeMultiTileBarrier(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0;
}

template <>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    return sizeof(RENDER_SURFACE_STATE);
}

template <>
void CommandListCoreFamily<gfxCoreFamily>::adjustWriteKernelTimestamp(uint64_t globalAddress, uint64_t baseAddress, CommandToPatchContainer *outTimeStampSyncCmds,
                                                                      bool workloadPartition, bool copyOperation, bool globalTimestamp) {}

template <>
bool CommandListCoreFamily<gfxCoreFamily>::isInOrderNonWalkerSignalingRequired(const Event *event) const {
    return false;
}

template <>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(Kernel *kernel,
                                                                               const ze_group_count_t &threadGroupDimensions,
                                                                               Event *event,
                                                                               CmdListKernelLaunchParams &launchParams) {
    UNRECOVERABLE_IF(kernel == nullptr);
    UNRECOVERABLE_IF(launchParams.skipInOrderNonWalkerSignaling);
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    const auto &kernelDescriptor = kernel->getKernelDescriptor();
    if (kernelDescriptor.kernelAttributes.flags.isInvalid) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const auto kernelImmutableData = kernel->getImmutableData();
    auto kernelInfo = kernelImmutableData->getKernelInfo();

    NEO::IndirectHeap *ssh = nullptr;
    NEO::IndirectHeap *dsh = nullptr;

    DBG_LOG(PrintDispatchParameters, "Kernel: ", kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
            ", Group size: ", kernel->getGroupSize()[0], ", ", kernel->getGroupSize()[1], ", ", kernel->getGroupSize()[2],
            ", Group count: ", threadGroupDimensions.groupCountX, ", ", threadGroupDimensions.groupCountY, ", ", threadGroupDimensions.groupCountZ,
            ", SIMD: ", kernelInfo->getMaxSimdSize());

    if (this->immediateCmdListHeapSharing || this->stateBaseAddressTracking) {
        auto &sshReserveConfig = commandContainer.getSurfaceStateHeapReserve();
        NEO::HeapReserveArguments sshReserveArgs = {
            sshReserveConfig.indirectHeapReservation,
            NEO::EncodeDispatchKernel<GfxFamily>::getSizeRequiredSsh(*kernelInfo),
            NEO::EncodeDispatchKernel<GfxFamily>::getDefaultSshAlignment()};

        // update SSH size - when global bindless addressing is used, kernel args may not require ssh space
        if (kernel->getSurfaceStateHeapDataSize() == 0) {
            sshReserveArgs.size = 0;
        }

        auto &dshReserveConfig = commandContainer.getDynamicStateHeapReserve();
        NEO::HeapReserveArguments dshReserveArgs = {
            dshReserveConfig.indirectHeapReservation,
            NEO::EncodeDispatchKernel<GfxFamily>::getSizeRequiredDsh(kernelDescriptor, commandContainer.getNumIddPerBlock()),
            NEO::EncodeDispatchKernel<GfxFamily>::getDefaultDshAlignment()};

        if (launchParams.isKernelSplitOperation) {
            // when appendLaunchKernel is called during an operation with kernel split is true,
            // then reserve sufficient ssh and dsh heaps during first kernel split, by multiplying, individual
            // dsh and ssh heap size retrieved above with number of kernels in split operation.
            // And after first kernel split, for remainder kernel split calls, dont estimate heap size.
            if (launchParams.numKernelsExecutedInSplitLaunch == 0) {
                dshReserveArgs.size = launchParams.numKernelsInSplitLaunch * dshReserveArgs.size;
                sshReserveArgs.size = launchParams.numKernelsInSplitLaunch * sshReserveArgs.size;
                commandContainer.reserveSpaceForDispatch(
                    sshReserveArgs,
                    dshReserveArgs, true);
            }
        } else {
            commandContainer.reserveSpaceForDispatch(
                sshReserveArgs,
                dshReserveArgs, true);
        }
        ssh = sshReserveArgs.indirectHeapReservation;
        dsh = dshReserveArgs.indirectHeapReservation;
    }

    appendEventForProfiling(event, nullptr, true, false, false, false);

    auto perThreadScratchSize = std::max<std::uint32_t>(this->getCommandListPerThreadScratchSize(0u),
                                                        kernel->getImmutableData()->getDescriptor().kernelAttributes.perThreadScratchSize[0]);
    this->setCommandListPerThreadScratchSize(0u, perThreadScratchSize);

    auto slmEnable = (kernel->getImmutableData()->getDescriptor().kernelAttributes.slmInlineSize > 0);
    this->setCommandListSLMEnable(slmEnable);

    auto kernelPreemptionMode = obtainKernelPreemptionMode(kernel);
    commandListPreemptionMode = std::min(commandListPreemptionMode, kernelPreemptionMode);

    kernel->patchGlobalOffset();

    this->allocateOrReuseKernelPrivateMemoryIfNeeded(kernel, kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize);

    if (!launchParams.isIndirect) {
        kernel->setGroupCount(threadGroupDimensions.groupCountX,
                              threadGroupDimensions.groupCountY,
                              threadGroupDimensions.groupCountZ);
    }

    if (launchParams.isIndirect) {
        prepareIndirectParams(&threadGroupDimensions);
    }

    if (kernel->hasIndirectAllocationsAllowed()) {
        UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();

        if (unifiedMemoryControls.indirectDeviceAllocationsAllowed) {
            this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
        }
        if (unifiedMemoryControls.indirectHostAllocationsAllowed) {
            this->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
        }
        if (unifiedMemoryControls.indirectSharedAllocationsAllowed) {
            this->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
        }

        this->indirectAllocationsAllowed = true;
    }

    containsCooperativeKernelsFlag = (containsCooperativeKernelsFlag || launchParams.isCooperative);
    if (kernel->usesSyncBuffer()) {
        auto retVal = (launchParams.isCooperative
                           ? programSyncBuffer(*kernel, *device->getNEODevice(), threadGroupDimensions, launchParams.syncBufferPatchIndex)
                           : ZE_RESULT_ERROR_INVALID_ARGUMENT);
        if (retVal) {
            return retVal;
        }
    }

    KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    bool uncachedMocsKernel = isKernelUncachedMocsRequired(kernelImp->getKernelRequiresUncachedMocs());
    this->requiresQueueUncachedMocs |= kernelImp->getKernelRequiresQueueUncachedMocs();

    NEO::Device *neoDevice = device->getNEODevice();

    auto localMemSize = static_cast<uint32_t>(neoDevice->getDeviceInfo().localMemSize);
    auto slmTotalSize = kernelImp->getSlmTotalSize();
    if (slmTotalSize > 0 && localMemSize < slmTotalSize) {
        CREATE_DEBUG_STRING(str, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if (this->swTagsEnabled) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::KernelNameTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            kernel->getKernelDescriptor().kernelMetadata.kernelName.c_str(), 0u);
    }

    std::list<void *> additionalCommands;

    updateStreamProperties(*kernel, launchParams.isCooperative, threadGroupDimensions, launchParams.isIndirect);

    auto maxWgCountPerTile = kernel->getMaxWgCountPerTile(this->engineGroupType);

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        .device = neoDevice,
        .dispatchInterface = kernel,
        .surfaceStateHeap = ssh,
        .dynamicStateHeap = dsh,
        .threadGroupDimensions = reinterpret_cast<const void *>(&threadGroupDimensions),
        .outWalkerPtr = nullptr,
        .outWalkerGpuVa = 0,
        .cpuWalkerBuffer = nullptr,
        .cpuPayloadBuffer = nullptr,
        .outImplicitArgsPtr = nullptr,
        .outImplicitArgsGpuVa = 0,
        .additionalCommands = &additionalCommands,
        .extendedArgs = nullptr,
        .postSyncArgs = {
            .eventAddress = 0,
            .postSyncImmValue = static_cast<uint64_t>(Event::STATE_SIGNALED),
            .inOrderCounterValue = 0,
            .inOrderIncrementGpuAddress = 0,
            .inOrderIncrementValue = 0,
            .device = neoDevice,
            .inOrderExecInfo = nullptr,
            .isCounterBasedEvent = false,
            .isTimestampEvent = false,
            .isHostScopeSignalEvent = false,
            .isUsingSystemAllocation = false,
            .dcFlushEnable = this->dcFlushSupport,
            .interruptEvent = false,
            .isFlushL3ForExternalAllocationRequired = false,
            .isFlushL3ForHostUsmRequired = false,
        },
        .preemptionMode = commandListPreemptionMode,
        .requiredPartitionDim = launchParams.requiredPartitionDim,
        .requiredDispatchWalkOrder = launchParams.requiredDispatchWalkOrder,
        .localRegionSize = launchParams.localRegionSize,
        .partitionCount = 0,
        .reserveExtraPayloadSpace = launchParams.reserveExtraPayloadSpace,
        .maxWgCountPerTile = maxWgCountPerTile,
        .defaultPipelinedThreadArbitrationPolicy = NEO::ThreadArbitrationPolicy::NotPresent,
        .isIndirect = launchParams.isIndirect,
        .isPredicate = launchParams.isPredicate,
        .requiresUncachedMocs = uncachedMocsKernel,
        .isInternal = internalUsage,
        .isCooperative = launchParams.isCooperative,
        .isKernelDispatchedFromImmediateCmdList = isImmediateType(),
        .isRcs = engineGroupType == NEO::EngineGroupType::renderCompute,
        .isHeaplessModeEnabled = this->heaplessModeEnabled,
        .isHeaplessStateInitEnabled = this->heaplessStateInitEnabled,
        .immediateScratchAddressPatching = !this->scratchAddressPatchingEnabled,
        .makeCommandView = false,
    };

    NEO::EncodeDispatchKernel<GfxFamily>::encodeCommon(commandContainer, dispatchKernelArgs);
    if (!isImmediateType()) {
        this->containsStatelessUncachedResource = dispatchKernelArgs.requiresUncachedMocs;
    }

    if (neoDevice->getDebugger() && !this->immediateCmdListHeapSharing && !neoDevice->getBindlessHeapsHelper()) {
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
        args.areMultipleSubDevicesInContext = false;
        args.isDebuggerActive = true;
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
    }

    appendSignalEventPostWalker(event, nullptr, nullptr, false, false, false);

    commandContainer.addToResidencyContainer(kernelImmutableData->getIsaGraphicsAllocation());
    auto &argumentsResidencyContainer = kernel->getArgumentsResidencyContainer();
    this->addResidency(argumentsResidencyContainer);
    auto &internalResidencyContainer = kernel->getInternalResidencyContainer();
    this->addResidency(internalResidencyContainer);
    if (kernelImp->getPrintfBufferAllocation() != nullptr) {
        storePrintfKernel(kernel);
    }

    if (kernelDescriptor.kernelAttributes.flags.usesAssert) {
        kernelWithAssertAppended = true;
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), neoDevice->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        commandsToPatch.push_back({.pCommand = additionalCommands.front(), .type = CommandToPatch::PauseOnEnqueuePipeControlStart});
        additionalCommands.pop_front();
        commandsToPatch.push_back({.pCommand = additionalCommands.front(), .type = CommandToPatch::PauseOnEnqueueSemaphoreStart});
        additionalCommands.pop_front();
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), neoDevice->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        commandsToPatch.push_back({.pCommand = additionalCommands.front(), .type = CommandToPatch::PauseOnEnqueuePipeControlEnd});
        additionalCommands.pop_front();
        commandsToPatch.push_back({.pCommand = additionalCommands.front(), .type = CommandToPatch::PauseOnEnqueueSemaphoreEnd});
        additionalCommands.pop_front();
    }

    if (event != nullptr && kernel->getPrintfBufferAllocation() != nullptr) {
        auto module = static_cast<const ModuleImp *>(&static_cast<KernelImp *>(kernel)->getParentModule());
        event->setKernelForPrintf(module->getPrintfKernelWeakPtr(kernel->toHandle()));
        event->setKernelWithPrintfDeviceMutex(kernel->getDevicePrintfKernelMutex());
    }

    if (this->isInOrderExecutionEnabled() && !launchParams.isKernelSplitOperation) {
        if (!event || !event->getAllocation(this->device)) {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = getDcFlushRequired(true);

            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
        }
        appendSignalInOrderDependencyCounter(event, false, false, false, false);
    }

    return ZE_RESULT_SUCCESS;
}

template <>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionPrologue(uint32_t partitionDataSize) {}

template <>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionEpilogue() {}

template <>
void CommandListCoreFamily<gfxCoreFamily>::appendComputeBarrierCommand() {
    NEO::PipeControlArgs args = createBarrierFlags();
    NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
}

template <>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(Kernel *kernel,
                                                                          const ze_group_count_t &threadGroupDimensions,
                                                                          Event *event,
                                                                          CmdListKernelLaunchParams &launchParams) {
    return appendLaunchKernelWithParams(kernel, threadGroupDimensions, nullptr, launchParams);
}

template <>
void CommandListCoreFamily<gfxCoreFamily>::appendDispatchOffsetRegister(bool workloadPartitionEvent, bool beforeProfilingCmds) {
}

template <>
bool CommandListCoreFamily<gfxCoreFamily>::singleEventPacketRequired(bool inputSinglePacketEventRequest) const {
    return true;
}

template struct CommandListCoreFamily<gfxCoreFamily>;
template struct CommandListCoreFamilyImmediate<gfxCoreFamily>;

} // namespace L0
