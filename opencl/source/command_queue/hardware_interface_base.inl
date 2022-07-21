/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
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
inline typename GfxFamily::WALKER_TYPE *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream, const Kernel &kernel) {
    auto walkerCmd = commandStream.getSpaceForCmd<WALKER_TYPE>();
    return walkerCmd;
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(
    TagNodeBase *hwTimeStamps,
    TagNodeBase *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream, commandQueue.getDevice().getHardwareInfo());
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
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
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream, commandQueue.getDevice().getHardwareInfo());
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
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
                                mainKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics,
                                mainKernel->areMultipleSubDevicesInContext());
    }

    bool programDependencies = true;

    if (DebugManager.flags.ResolveDependenciesViaPipeControls.get() == 1) {
        //only optimize kernel after kernel
        if (commandQueue.peekLatestSentEnqueueOperation() == EnqueueProperties::Operation::GpuKernel) {
            programDependencies = false;
        }
    }

    if (programDependencies) {
        TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(*commandStream, csrDependencies);
    }

    dsh->align(EncodeStates<GfxFamily>::alignInterfaceDescriptorData);

    walkerArgs.interfaceDescriptorIndex = 0;
    walkerArgs.offsetInterfaceDescriptorTable = dsh->getUsed();

    size_t totalInterfaceDescriptorTableSize = sizeof(INTERFACE_DESCRIPTOR_DATA);

    getDefaultDshSpace(walkerArgs.offsetInterfaceDescriptorTable, commandQueue, multiDispatchInfo, totalInterfaceDescriptorTableSize, dsh, commandStream);

    // Program media interface descriptor load
    HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        walkerArgs.offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(walkerArgs.offsetInterfaceDescriptorTable % 64 != 0);

    dispatchProfilingPerfStartCommands(walkerArgs.hwTimeStamps, walkerArgs.hwPerfCounter, commandStream, commandQueue);

    const auto &hwInfo = commandQueue.getDevice().getHardwareInfo();
    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserStartConfirmation,
                                   DebugPauseState::hasUserStartConfirmation, hwInfo);
    }

    mainKernel->performKernelTuning(commandQueue.getGpgpuCommandStreamReceiver(),
                                    multiDispatchInfo.begin()->getLocalWorkgroupSize(),
                                    multiDispatchInfo.begin()->getActualWorkgroupSize(),
                                    multiDispatchInfo.begin()->getOffset(),
                                    walkerArgs.currentTimestampPacketNodes);

    walkerArgs.currentDispatchIndex = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        dispatchInfo.dispatchInitCommands(*commandStream, walkerArgs.timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo());
        walkerArgs.isMainKernel = (dispatchInfo.getKernel() == mainKernel);

        dispatchKernelCommands(commandQueue, dispatchInfo, *commandStream, *dsh, *ioh, *ssh, walkerArgs);

        walkerArgs.currentDispatchIndex++;
        dispatchInfo.dispatchEpilogueCommands(*commandStream, walkerArgs.timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo());
    }

    if (mainKernel->requiresCacheFlushCommand(commandQueue)) {
        uint64_t postSyncAddress = 0;
        if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            auto timestampPacketNodeForPostSync = walkerArgs.currentTimestampPacketNodes->peekNodes().at(walkerArgs.currentDispatchIndex);
            timestampPacketNodeForPostSync->setProfilingCapable(false);
            postSyncAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNodeForPostSync);
        }
        HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(commandStream, commandQueue, mainKernel, postSyncAddress);
    }

    if (PauseOnGpuProperties::gpuScratchRegWriteAllowed(DebugManager.flags.GpuScratchRegWriteAfterWalker.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount())) {
        uint32_t registerOffset = DebugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t registerData = DebugManager.flags.GpuScratchRegWriteRegisterData.get();
        LriHelper<GfxFamily>::program(commandStream, registerOffset, registerData, EncodeSetMMIO<GfxFamily>::isRemapApplicable(registerOffset));
    }

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserEndConfirmation,
                                   DebugPauseState::hasUserEndConfirmation, hwInfo);
    }

    dispatchProfilingPerfEndCommands(walkerArgs.hwTimeStamps, walkerArgs.hwPerfCounter, commandStream, commandQueue);
}

template <typename GfxFamily>
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

    //Get dispatch geometry
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

    programWalker(commandStream, kernel, commandQueue, dsh, ioh, ssh, dispatchInfo, walkerArgs);

    dispatchWorkarounds(&commandStream, commandQueue, kernel, false);
}

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::obtainIndirectHeaps(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo,
                                                       bool blockedQueue, IndirectHeap *&dsh, IndirectHeap *&ioh, IndirectHeap *&ssh) {
    if (blockedQueue) {
        size_t dshSize = 0;
        size_t colorCalcSize = 0;
        size_t sshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo);
        bool iohEqualsDsh = false;

        dshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo);

        commandQueue.allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, dshSize, dsh);
        dsh->getSpace(colorCalcSize);

        commandQueue.allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, sshSize, ssh);

        if (iohEqualsDsh) {
            ioh = dsh;
        } else {
            commandQueue.allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT,
                                            HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo), ioh);
        }
    } else {
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::Type::SURFACE_STATE>(commandQueue, multiDispatchInfo);
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
            const auto &hwInfo = commandQueue.getDevice().getHardwareInfo();
            PipeControlArgs args;
            args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
            MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandStream,
                PostSyncMode::ImmediateData,
                address,
                static_cast<uint64_t>(confirmationTrigger),
                hwInfo,
                args);
        }

        {
            using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
            using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
            EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(*commandStream,
                                                                  address,
                                                                  static_cast<uint32_t>(waitCondition),
                                                                  COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD);
        }
    }
}
} // namespace NEO
