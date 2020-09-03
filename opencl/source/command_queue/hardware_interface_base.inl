/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

template <typename GfxFamily>
inline WALKER_TYPE<GfxFamily> *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream,
                                                                                 const Kernel &kernel) {
    auto walkerCmd = commandStream.getSpaceForCmd<WALKER_TYPE<GfxFamily>>();
    return walkerCmd;
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
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
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
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
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
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
                                sizeToPatch, addressToPatch, 0, debugSurface, 0, 0);
    }

    auto numSupportedDevices = commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getNumSupportedDevices();
    TimestampPacketHelper::programCsrDependencies<GfxFamily>(*commandStream, csrDependencies, numSupportedDevices);

    dsh->align(HardwareCommandsHelper<GfxFamily>::alignInterfaceDescriptorData);

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
    dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserStartConfirmation, DebugPauseState::hasUserStartConfirmation);

    size_t currentDispatchIndex = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        dispatchInfo.dispatchInitCommands(*commandStream, timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo(), numSupportedDevices);
        bool isMainKernel = (dispatchInfo.getKernel() == mainKernel);

        dispatchKernelCommands(commandQueue, dispatchInfo, commandType, *commandStream, isMainKernel,
                               currentDispatchIndex, currentTimestampPacketNodes, preemptionMode, interfaceDescriptorIndex,
                               offsetInterfaceDescriptorTable, *dsh, *ioh, *ssh);

        currentDispatchIndex++;
        dispatchInfo.dispatchEpilogueCommands(*commandStream, timestampPacketDependencies, commandQueue.getDevice().getHardwareInfo(), numSupportedDevices);
    }
    if (mainKernel->requiresCacheFlushCommand(commandQueue)) {
        uint64_t postSyncAddress = 0;
        if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            auto timestampPacketNodeForPostSync = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
            postSyncAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNodeForPostSync);
        }
        HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(commandStream, commandQueue, mainKernel, postSyncAddress);
    }

    dispatchDebugPauseCommands(commandStream, commandQueue, DebugPauseState::waitingForUserEndConfirmation, DebugPauseState::hasUserEndConfirmation);
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
    Vec3<size_t> totalNumberOfWorkgroups = (dispatchInfo.getTotalNumberOfWorkgroups().x > 0) ? dispatchInfo.getTotalNumberOfWorkgroups()
                                                                                             : generateWorkgroupsNumber(gws, lws);

    Vec3<size_t> numberOfWorkgroups = (dispatchInfo.getNumberOfWorkgroups().x > 0) ? dispatchInfo.getNumberOfWorkgroups() : totalNumberOfWorkgroups;

    size_t globalWorkSizes[3] = {gws.x, gws.y, gws.z};

    // Patch our kernel constants
    *kernel.globalWorkOffsetX = static_cast<uint32_t>(offset.x);
    *kernel.globalWorkOffsetY = static_cast<uint32_t>(offset.y);
    *kernel.globalWorkOffsetZ = static_cast<uint32_t>(offset.z);

    *kernel.globalWorkSizeX = static_cast<uint32_t>(gws.x);
    *kernel.globalWorkSizeY = static_cast<uint32_t>(gws.y);
    *kernel.globalWorkSizeZ = static_cast<uint32_t>(gws.z);

    if (isMainKernel || (kernel.localWorkSizeX2 == &Kernel::dummyPatchLocation)) {
        *kernel.localWorkSizeX = static_cast<uint32_t>(lws.x);
        *kernel.localWorkSizeY = static_cast<uint32_t>(lws.y);
        *kernel.localWorkSizeZ = static_cast<uint32_t>(lws.z);
    }

    *kernel.localWorkSizeX2 = static_cast<uint32_t>(lws.x);
    *kernel.localWorkSizeY2 = static_cast<uint32_t>(lws.y);
    *kernel.localWorkSizeZ2 = static_cast<uint32_t>(lws.z);

    *kernel.enqueuedLocalWorkSizeX = static_cast<uint32_t>(elws.x);
    *kernel.enqueuedLocalWorkSizeY = static_cast<uint32_t>(elws.y);
    *kernel.enqueuedLocalWorkSizeZ = static_cast<uint32_t>(elws.z);

    if (isMainKernel) {
        *kernel.numWorkGroupsX = static_cast<uint32_t>(totalNumberOfWorkgroups.x);
        *kernel.numWorkGroupsY = static_cast<uint32_t>(totalNumberOfWorkgroups.y);
        *kernel.numWorkGroupsZ = static_cast<uint32_t>(totalNumberOfWorkgroups.z);
    }

    *kernel.workDim = dim;

    // Send our indirect object data
    size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

    dispatchWorkarounds(&commandStream, commandQueue, kernel, true);

    if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNode = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, nullptr, timestampPacketNode, TimestampPacketStorage::WriteOperationType::BeforeWalker, commandQueue.getDevice().getRootDeviceEnvironment());
    }

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

    if (static_cast<int32_t>(commandQueue.getGpgpuCommandStreamReceiver().peekTaskCount()) == DebugManager.flags.PauseOnEnqueue.get() &&
        !commandQueue.isSpecial()) {
        auto address = commandQueue.getGpgpuCommandStreamReceiver().getDebugPauseStateGPUAddress();
        {
            using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
            using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

            auto pCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
            *pCmd = GfxFamily::cmdInitPipeControl;

            pCmd->setCommandStreamerStallEnable(true);
            pCmd->setDcFlushEnable(true);
            pCmd->setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
            pCmd->setAddressHigh(static_cast<uint32_t>(address >> 32));
            pCmd->setPostSyncOperation(POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
            pCmd->setImmediateData(static_cast<uint32_t>(confirmationTrigger));
        }

        {
            using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

            auto pCmd = (MI_SEMAPHORE_WAIT *)commandStream->getSpace(sizeof(MI_SEMAPHORE_WAIT));
            *pCmd = GfxFamily::cmdInitMiSemaphoreWait;

            pCmd->setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD);
            pCmd->setSemaphoreDataDword(static_cast<uint32_t>(waitCondition));
            pCmd->setSemaphoreGraphicsAddress(address);
            pCmd->setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
        }
    }
}

} // namespace NEO
