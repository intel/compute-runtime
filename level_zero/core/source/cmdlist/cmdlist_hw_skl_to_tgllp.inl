/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

#include "encode_surface_state_args.h"

#include <algorithm>

namespace L0 {
struct DeviceImp;

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamily<gfxCoreFamily>::getReserveSshSize() {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    return sizeof(RENDER_SURFACE_STATE);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::adjustWriteKernelTimestamp(uint64_t globalAddress, uint64_t contextAddress, uint64_t baseAddress, CommandToPatchContainer *outTimeStampSyncCmds,
                                                                      bool workloadPartition, bool copyOperation) {}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isInOrderNonWalkerSignalingRequired(const Event *event) const {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
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

    if (NEO::debugManager.flags.EnableSWTags.get()) {
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
        .cpuWalkerBuffer = nullptr,
        .cpuPayloadBuffer = nullptr,
        .outImplicitArgsPtr = nullptr,
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
    for (auto resource : argumentsResidencyContainer) {
        commandContainer.addToResidencyContainer(resource);
    }
    auto &internalResidencyContainer = kernel->getInternalResidencyContainer();
    for (auto resource : internalResidencyContainer) {
        commandContainer.addToResidencyContainer(resource);
    }

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
        appendSignalInOrderDependencyCounter(event, false, false, false);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionPrologue(uint32_t partitionDataSize) {}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendMultiPartitionEpilogue() {}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendComputeBarrierCommand() {
    NEO::PipeControlArgs args = createBarrierFlags();
    NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline NEO::PipeControlArgs CommandListCoreFamily<gfxCoreFamily>::createBarrierFlags() {
    NEO::PipeControlArgs args;
    return args;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandListCoreFamily<gfxCoreFamily>::appendMultiTileBarrier(NEO::Device &neoDevice) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::estimateBufferSizeMultiTileBarrier(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(Kernel *kernel,
                                                                          const ze_group_count_t &threadGroupDimensions,
                                                                          Event *event,
                                                                          CmdListKernelLaunchParams &launchParams) {
    return appendLaunchKernelWithParams(kernel, threadGroupDimensions, nullptr, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline NEO::PreemptionMode CommandListCoreFamily<gfxCoreFamily>::obtainKernelPreemptionMode(Kernel *kernel) {
    NEO::PreemptionFlags flags = NEO::PreemptionHelper::createPreemptionLevelFlags(*device->getNEODevice(), &kernel->getImmutableData()->getDescriptor());
    return NEO::PreemptionHelper::taskPreemptionMode(device->getDevicePreemptionMode(), flags);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendDispatchOffsetRegister(bool workloadPartitionEvent, bool beforeProfilingCmds) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::singleEventPacketRequired(bool inputSinglePacketEventRequest) const {
    return true;
}

} // namespace L0
