/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

template <typename GfxFamily>
template <typename WalkerType>
inline WalkerType *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream, const Kernel &kernel) {
    auto walkerCmd = commandStream.getSpaceForCmd<WalkerType>();
    return walkerCmd;
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchWalkerCommon(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs) {

    dispatchWalker<typename GfxFamily::DefaultWalkerType>(commandQueue, multiDispatchInfo, csrDependencies, walkerArgs);
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(
    TagNodeBase *hwTimeStamps,
    TagNodeBase *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
    }
    // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream, commandQueue.getDevice().getRootDeviceEnvironment());
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfEndCommands(
    TagNodeBase *hwTimeStamps,
    TagNodeBase *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream, commandQueue.getDevice().getRootDeviceEnvironment());
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
template <typename WalkerType>
void HardwareInterface<GfxFamily>::dispatchWalker(
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    const CsrDependencies &csrDependencies,
    HardwareInterfaceWalkerArgs &walkerArgs) {

    LinearStream *commandStream = nullptr;
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    auto mainKernel = multiDispatchInfo.peekMainKernel();
    walkerArgs.preemptionMode = ClPreemptionHelper::taskPreemptionMode(commandQueue.getDevice(), multiDispatchInfo);

    for (auto &dispatchInfo : multiDispatchInfo) {
        // Compute local workgroup sizes
        if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
            const auto lws = generateWorkgroupSize(dispatchInfo);
            const_cast<DispatchInfo &>(dispatchInfo).setLWS(lws);
        }
        if (dispatchInfo.getKernel() == mainKernel) {
            if (!mainKernel->isLocalWorkSize2Patchable()) {
                const auto &lws = dispatchInfo.getLocalWorkgroupSize();
                mainKernel->setLocalWorkSizeValues(static_cast<uint32_t>(lws.x), static_cast<uint32_t>(lws.y), static_cast<uint32_t>(lws.z));
            }
        }
    }

    // Allocate command stream and indirect heaps
    bool blockedQueue = (walkerArgs.blockedCommandsData != nullptr);
    obtainIndirectHeaps(commandQueue, multiDispatchInfo, blockedQueue, dsh, ioh, ssh);
    if (blockedQueue) {
        walkerArgs.blockedCommandsData->setHeaps(dsh, ioh, ssh);
        commandStream = walkerArgs.blockedCommandsData->commandStream.get();
    } else {
        commandStream = &commandQueue.getCS(0);
    }

    if (commandQueue.getDevice().getDebugger()) {
        auto debugSurface = commandQueue.getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();
        void *addressToPatch = reinterpret_cast<void *>(debugSurface->getGpuAddress());
        size_t sizeToPatch = debugSurface->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&commandQueue.getDevice(), commandQueue.getDevice().getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh),
                                false, false, sizeToPatch, addressToPatch, 0, debugSurface, 0, 0,
                                mainKernel->areMultipleSubDevicesInContext());
    }

    if (walkerArgs.relaxedOrderingEnabled) {
        RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(*commandStream, false);
    }

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(*commandStream, csrDependencies, walkerArgs.relaxedOrderingEnabled, commandQueue.isBcs());

    dsh->align(NEO::EncodeDispatchKernel<GfxFamily>::getDefaultDshAlignment());

    walkerArgs.interfaceDescriptorIndex = 0;
    walkerArgs.offsetInterfaceDescriptorTable = dsh->getUsed();

    size_t totalInterfaceDescriptorTableSize = GfxFamily::template getInterfaceDescriptorSize<WalkerType>();

    getDefaultDshSpace(walkerArgs.offsetInterfaceDescriptorTable, commandQueue, multiDispatchInfo, totalInterfaceDescriptorTableSize, dsh, commandStream);

    // Program media interface descriptor load
    HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        walkerArgs.offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(walkerArgs.offsetInterfaceDescriptorTable % 64 != 0);

    dispatchProfilingPerfStartCommands(walkerArgs.hwTimeStamps, walkerArgs.hwPerfCounter, commandStream, commandQueue);

    const auto &hwInfo = commandQueue.getDevice().getHardwareInfo();
    if (PauseOnGpuProperties::pauseModeAllowed(debugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserStartConfirmation,
                                   DebugPauseState::hasUserStartConfirmation, hwInfo);
    }

    walkerArgs.currentDispatchIndex = 0;

    for (auto &dispatchInfo : multiDispatchInfo) {
        dispatchInfo.dispatchInitCommands(*commandStream, walkerArgs.timestampPacketDependencies, commandQueue.getDevice().getRootDeviceEnvironment());
        walkerArgs.isMainKernel = (dispatchInfo.getKernel() == mainKernel);

        dispatchKernelCommands<WalkerType>(commandQueue, dispatchInfo, *commandStream, *dsh, *ioh, *ssh, walkerArgs);

        walkerArgs.currentDispatchIndex++;
        dispatchInfo.dispatchEpilogueCommands(*commandStream, walkerArgs.timestampPacketDependencies, commandQueue.getDevice().getRootDeviceEnvironment());
    }

    if (PauseOnGpuProperties::gpuScratchRegWriteAllowed(debugManager.flags.GpuScratchRegWriteAfterWalker.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount())) {
        uint32_t registerOffset = debugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t registerData = debugManager.flags.GpuScratchRegWriteRegisterData.get();

        PipeControlArgs args;
        args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, commandQueue.getDevice().getRootDeviceEnvironment());
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *commandStream,
            PostSyncMode::noWrite,
            0u,
            0u,
            commandQueue.getDevice().getRootDeviceEnvironment(),
            args);
        LriHelper<GfxFamily>::program(commandStream, registerOffset, registerData, EncodeSetMMIO<GfxFamily>::isRemapApplicable(registerOffset), commandQueue.isBcs());
    }

    if (PauseOnGpuProperties::pauseModeAllowed(debugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserEndConfirmation,
                                   DebugPauseState::hasUserEndConfirmation, hwInfo);
    }

    dispatchProfilingPerfEndCommands(walkerArgs.hwTimeStamps, walkerArgs.hwPerfCounter, commandStream, commandQueue);
}

template <typename GfxFamily>
template <typename WalkerType>
void HardwareInterface<GfxFamily>::dispatchKernelCommands(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream,
                                                          IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh,
                                                          HardwareInterfaceWalkerArgs &walkerArgs) {
    auto &kernel = *dispatchInfo.getKernel();
    DEBUG_BREAK_IF(!(dispatchInfo.getDim() >= 1 && dispatchInfo.getDim() <= 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getGWS().z == 1 || dispatchInfo.getDim() == 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getGWS().y == 1 || dispatchInfo.getDim() >= 2));
    DEBUG_BREAK_IF(!(dispatchInfo.getOffset().z == 0 || dispatchInfo.getDim() == 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getOffset().y == 0 || dispatchInfo.getDim() >= 2));

    // If we don't have a required WGS, compute one opportunistically
    if (walkerArgs.commandType == CL_COMMAND_NDRANGE_KERNEL) {
        provideLocalWorkGroupSizeHints(commandQueue.getContextPtr(), dispatchInfo);
    }

    // Get dispatch geometry
    auto dim = dispatchInfo.getDim();
    const auto &gws = dispatchInfo.getGWS();
    const auto &offset = dispatchInfo.getOffset();
    walkerArgs.startOfWorkgroups = &dispatchInfo.getStartOfWorkgroups();

    // Compute local workgroup sizes
    const auto &lws = dispatchInfo.getLocalWorkgroupSize();
    const auto &elws = (dispatchInfo.getEnqueuedWorkgroupSize().x > 0) ? dispatchInfo.getEnqueuedWorkgroupSize() : lws;

    // Compute number of work groups
    const auto &totalNumberOfWorkgroups = dispatchInfo.getTotalNumberOfWorkgroups();
    walkerArgs.numberOfWorkgroups = &dispatchInfo.getNumberOfWorkgroups();
    UNRECOVERABLE_IF(totalNumberOfWorkgroups.x == 0);
    UNRECOVERABLE_IF(walkerArgs.numberOfWorkgroups->x == 0);

    walkerArgs.globalWorkSizes[0] = gws.x;
    walkerArgs.globalWorkSizes[1] = gws.y;
    walkerArgs.globalWorkSizes[2] = gws.z;

    // Patch our kernel constants
    kernel.setGlobalWorkOffsetValues(static_cast<uint32_t>(offset.x), static_cast<uint32_t>(offset.y), static_cast<uint32_t>(offset.z));
    kernel.setGlobalWorkSizeValues(static_cast<uint32_t>(gws.x), static_cast<uint32_t>(gws.y), static_cast<uint32_t>(gws.z));

    if (walkerArgs.isMainKernel || (!kernel.isLocalWorkSize2Patchable())) {
        kernel.setLocalWorkSizeValues(static_cast<uint32_t>(lws.x), static_cast<uint32_t>(lws.y), static_cast<uint32_t>(lws.z));
    }

    kernel.setLocalWorkSize2Values(static_cast<uint32_t>(lws.x), static_cast<uint32_t>(lws.y), static_cast<uint32_t>(lws.z));
    kernel.setEnqueuedLocalWorkSizeValues(static_cast<uint32_t>(elws.x), static_cast<uint32_t>(elws.y), static_cast<uint32_t>(elws.z));

    if (walkerArgs.isMainKernel) {
        kernel.setNumWorkGroupsValues(static_cast<uint32_t>(totalNumberOfWorkgroups.x), static_cast<uint32_t>(totalNumberOfWorkgroups.y), static_cast<uint32_t>(totalNumberOfWorkgroups.z));
    }

    kernel.setWorkDim(dim);

    // Send our indirect object data
    walkerArgs.localWorkSizes[0] = lws.x;
    walkerArgs.localWorkSizes[1] = lws.y;
    walkerArgs.localWorkSizes[2] = lws.z;

    dispatchWorkarounds(&commandStream, commandQueue, kernel, true);

    programWalker<WalkerType>(commandStream, kernel, commandQueue, dsh, ioh, ssh, dispatchInfo, walkerArgs);

    dispatchWorkarounds(&commandStream, commandQueue, kernel, false);
}

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::obtainIndirectHeaps(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo,
                                                       bool blockedQueue, IndirectHeap *&dsh, IndirectHeap *&ioh, IndirectHeap *&ssh) {
    if (blockedQueue) {
        size_t dshSize = 0;
        size_t colorCalcSize = 0;
        size_t sshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo);

        dshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo);

        commandQueue.allocateHeapMemory(IndirectHeap::Type::dynamicState, dshSize, dsh);
        dsh->getSpace(colorCalcSize);

        commandQueue.allocateHeapMemory(IndirectHeap::Type::surfaceState, sshSize, ssh);
        commandQueue.allocateHeapMemory(IndirectHeap::Type::indirectObject,
                                        HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo), ioh);

    } else {
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::dynamicState>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::indirectObject>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::surfaceState>(commandQueue, multiDispatchInfo);
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchDebugPauseCommands(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    DebugPauseState confirmationTrigger,
    DebugPauseState waitCondition,
    const HardwareInfo &hwInfo) {

    if (!commandQueue.isSpecial()) {
        auto address = commandQueue.getGpgpuCommandStreamReceiver().getDebugPauseStateGPUAddress();
        {
            PipeControlArgs args;
            args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, commandQueue.getDevice().getRootDeviceEnvironment());
            MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandStream,
                PostSyncMode::immediateData,
                address,
                static_cast<uint64_t>(confirmationTrigger),
                commandQueue.getDevice().getRootDeviceEnvironment(),
                args);
        }

        {
            using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
            EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(*commandStream,
                                                                  address,
                                                                  static_cast<uint32_t>(waitCondition),
                                                                  COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        }
    }
}
} // namespace NEO
