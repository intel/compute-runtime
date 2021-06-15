/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"

#include "pipe_control_args.h"

namespace NEO {

template <typename GfxFamily>
inline WALKER_TYPE<GfxFamily> *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream,
                                                                                 const Kernel &kernel) {
    auto walkerCmd = commandStream.getSpaceForCmd<WALKER_TYPE<GfxFamily>>();
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
    KernelOperation *blockedCommandsData,
    TagNodeBase *hwTimeStamps,
    TagNodeBase *hwPerfCounter,
    TimestampPacketDependencies *timestampPacketDependencies,
    TimestampPacketContainer *currentTimestampPacketNodes,
    uint32_t commandType) {

    LinearStream *commandStream = nullptr;
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    auto parentKernel = multiDispatchInfo.peekParentKernel();
    auto mainKernel = multiDispatchInfo.peekMainKernel();
    auto preemptionMode = PreemptionHelper::taskPreemptionMode(commandQueue.getDevice(), multiDispatchInfo);

    for (auto &dispatchInfo : multiDispatchInfo) {
        // Compute local workgroup sizes
        if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
            const auto lws = generateWorkgroupSize(dispatchInfo);
            const_cast<DispatchInfo &>(dispatchInfo).setLWS(lws);
        }
    }

    // Allocate command stream and indirect heaps
    bool blockedQueue = (blockedCommandsData != nullptr);
    obtainIndirectHeaps(commandQueue, multiDispatchInfo, blockedQueue, dsh, ioh, ssh);
    if (blockedQueue) {
        blockedCommandsData->setHeaps(dsh, ioh, ssh);
        commandStream = blockedCommandsData->commandStream.get();
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

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(*commandStream, csrDependencies);

    dsh->align(EncodeStates<GfxFamily>::alignInterfaceDescriptorData);

    uint32_t interfaceDescriptorIndex = 0;
    const size_t offsetInterfaceDescriptorTable = dsh->getUsed();

    size_t totalInterfaceDescriptorTableSize = sizeof(INTERFACE_DESCRIPTOR_DATA);

    getDefaultDshSpace(offsetInterfaceDescriptorTable, commandQueue, multiDispatchInfo, totalInterfaceDescriptorTableSize,
                       parentKernel, dsh, commandStream);

    // Program media interface descriptor load
    HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    dispatchProfilingPerfStartCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserStartConfirmation, DebugPauseState::hasUserStartConfirmation);
    }

    mainKernel->performKernelTuning(commandQueue.getGpgpuCommandStreamReceiver(),
                                    multiDispatchInfo.begin()->getLocalWorkgroupSize(),
                                    multiDispatchInfo.begin()->getActualWorkgroupSize(),
                                    multiDispatchInfo.begin()->getOffset(),
                                    currentTimestampPacketNodes);

    size_t currentDispatchIndex = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        dispatchInfo.dispatchInitCommands(*commandStream, timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo());
        bool isMainKernel = (dispatchInfo.getKernel() == mainKernel);

        dispatchKernelCommands(commandQueue, dispatchInfo, commandType, *commandStream, isMainKernel,
                               currentDispatchIndex, currentTimestampPacketNodes, preemptionMode, interfaceDescriptorIndex,
                               offsetInterfaceDescriptorTable, *dsh, *ioh, *ssh);

        currentDispatchIndex++;
        dispatchInfo.dispatchEpilogueCommands(*commandStream, timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo());
    }

    if (mainKernel->requiresCacheFlushCommand(commandQueue)) {
        uint64_t postSyncAddress = 0;
        if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            auto timestampPacketNodeForPostSync = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
            timestampPacketNodeForPostSync->setProfilingCapable(false);
            postSyncAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNodeForPostSync);
        }
        HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(commandStream, commandQueue, mainKernel, postSyncAddress);
    }

    if (PauseOnGpuProperties::GpuScratchRegWriteAllowed(DebugManager.flags.GpuScratchRegWriteAfterWalker.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount())) {
        uint32_t registerOffset = DebugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t registerData = DebugManager.flags.GpuScratchRegWriteRegisterData.get();
        LriHelper<GfxFamily>::program(commandStream, registerOffset, registerData, EncodeSetMMIO<GfxFamily>::isRemapApplicable(registerOffset));
    }

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnEnqueue.get(), commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount(), PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserEndConfirmation, DebugPauseState::hasUserEndConfirmation);
    }

    dispatchProfilingPerfEndCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);
}

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::dispatchKernelCommands(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, uint32_t commandType,
                                                          LinearStream &commandStream, bool isMainKernel, size_t currentDispatchIndex,
                                                          TimestampPacketContainer *currentTimestampPacketNodes, PreemptionMode preemptionMode,
                                                          uint32_t &interfaceDescriptorIndex, size_t offsetInterfaceDescriptorTable,
                                                          IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh) {
    auto &kernel = *dispatchInfo.getKernel();
    DEBUG_BREAK_IF(!(dispatchInfo.getDim() >= 1 && dispatchInfo.getDim() <= 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getGWS().z == 1 || dispatchInfo.getDim() == 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getGWS().y == 1 || dispatchInfo.getDim() >= 2));
    DEBUG_BREAK_IF(!(dispatchInfo.getOffset().z == 0 || dispatchInfo.getDim() == 3));
    DEBUG_BREAK_IF(!(dispatchInfo.getOffset().y == 0 || dispatchInfo.getDim() >= 2));

    // If we don't have a required WGS, compute one opportunistically
    if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
        provideLocalWorkGroupSizeHints(commandQueue.getContextPtr(), dispatchInfo);
    }

    //Get dispatch geometry
    uint32_t dim = dispatchInfo.getDim();
    Vec3<size_t> gws = dispatchInfo.getGWS();
    Vec3<size_t> offset = dispatchInfo.getOffset();
    Vec3<size_t> startOfWorkgroups = dispatchInfo.getStartOfWorkgroups();

    // Compute local workgroup sizes
    Vec3<size_t> lws = dispatchInfo.getLocalWorkgroupSize();
    Vec3<size_t> elws = (dispatchInfo.getEnqueuedWorkgroupSize().x > 0) ? dispatchInfo.getEnqueuedWorkgroupSize() : lws;

    // Compute number of work groups
    Vec3<size_t> totalNumberOfWorkgroups = dispatchInfo.getTotalNumberOfWorkgroups();
    Vec3<size_t> numberOfWorkgroups = dispatchInfo.getNumberOfWorkgroups();
    UNRECOVERABLE_IF(totalNumberOfWorkgroups.x == 0);
    UNRECOVERABLE_IF(numberOfWorkgroups.x == 0);

    size_t globalWorkSizes[3] = {gws.x, gws.y, gws.z};

    // Patch our kernel constants
    kernel.setGlobalWorkOffsetValues(static_cast<uint32_t>(offset.x), static_cast<uint32_t>(offset.y), static_cast<uint32_t>(offset.z));
    kernel.setGlobalWorkSizeValues(static_cast<uint32_t>(gws.x), static_cast<uint32_t>(gws.y), static_cast<uint32_t>(gws.z));

    if (isMainKernel || (!kernel.isLocalWorkSize2Patchable())) {
        kernel.setLocalWorkSizeValues(static_cast<uint32_t>(lws.x), static_cast<uint32_t>(lws.y), static_cast<uint32_t>(lws.z));
    }

    kernel.setLocalWorkSize2Values(static_cast<uint32_t>(lws.x), static_cast<uint32_t>(lws.y), static_cast<uint32_t>(lws.z));
    kernel.setEnqueuedLocalWorkSizeValues(static_cast<uint32_t>(elws.x), static_cast<uint32_t>(elws.y), static_cast<uint32_t>(elws.z));

    if (isMainKernel) {
        kernel.setNumWorkGroupsValues(static_cast<uint32_t>(totalNumberOfWorkgroups.x), static_cast<uint32_t>(totalNumberOfWorkgroups.y), static_cast<uint32_t>(totalNumberOfWorkgroups.z));
    }

    kernel.setWorkDim(dim);

    // Send our indirect object data
    size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

    dispatchWorkarounds(&commandStream, commandQueue, kernel, true);

    programWalker(commandStream, kernel, commandQueue, currentTimestampPacketNodes, dsh, ioh, ssh, globalWorkSizes,
                  localWorkSizes, preemptionMode, currentDispatchIndex, interfaceDescriptorIndex, dispatchInfo,
                  offsetInterfaceDescriptorTable, numberOfWorkgroups, startOfWorkgroups);

    dispatchWorkarounds(&commandStream, commandQueue, kernel, false);
}

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::obtainIndirectHeaps(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo,
                                                       bool blockedQueue, IndirectHeap *&dsh, IndirectHeap *&ioh, IndirectHeap *&ssh) {
    auto parentKernel = multiDispatchInfo.peekParentKernel();

    if (blockedQueue) {
        size_t dshSize = 0;
        size_t colorCalcSize = 0;
        size_t sshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo);
        bool iohEqualsDsh = false;

        if (parentKernel) {
            dshSize = commandQueue.getContext().getDefaultDeviceQueue()->getDshBuffer()->getUnderlyingBufferSize();
            sshSize += HardwareCommandsHelper<GfxFamily>::getSshSizeForExecutionModel(*parentKernel);
            iohEqualsDsh = true;
            colorCalcSize = static_cast<size_t>(commandQueue.getContext().getDefaultDeviceQueue()->colorCalcStateSize);
        } else {
            dshSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo);
        }

        commandQueue.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
        dsh->getSpace(colorCalcSize);

        commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE, sshSize, ssh);

        if (iohEqualsDsh) {
            ioh = dsh;
        } else {
            commandQueue.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT,
                                            HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo), ioh);
        }
    } else {
        if (parentKernel && (commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0).getUsed() > 0)) {
            commandQueue.releaseIndirectHeap(IndirectHeap::SURFACE_STATE);
            // clean reserved bindless offsets
            ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
            ssh->replaceBuffer(ssh->getCpuBase(), ssh->getMaxAvailableSpace());
        }
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchDebugPauseCommands(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    DebugPauseState confirmationTrigger,
    DebugPauseState waitCondition) {

    if (!commandQueue.isSpecial()) {
        auto address = commandQueue.getGpgpuCommandStreamReceiver().getDebugPauseStateGPUAddress();
        {
            using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
            using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

            PipeControlArgs args(true);
            MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
                *commandStream,
                POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                address,
                static_cast<uint64_t>(confirmationTrigger),
                commandQueue.getDevice().getHardwareInfo(),
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
