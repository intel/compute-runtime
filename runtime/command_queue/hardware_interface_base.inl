/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/task_information.h"
#include "runtime/memory_manager/internal_allocation_storage.h"

namespace NEO {

template <typename GfxFamily>
inline WALKER_TYPE<GfxFamily> *HardwareInterface<GfxFamily>::allocateWalkerSpace(LinearStream &commandStream,
                                                                                 const Kernel &kernel) {
    auto walkerCmd = static_cast<WALKER_TYPE<GfxFamily> *>(commandStream.getSpace(sizeof(WALKER_TYPE<GfxFamily>)));
    *walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    return walkerCmd;
}

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::dispatchWalker(
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    const CsrDependencies &csrDependencies,
    KernelOperation *blockedCommandsData,
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
    TimestampPacketContainer *previousTimestampPacketNodes,
    TimestampPacketContainer *currentTimestampPacketNodes,
    PreemptionMode preemptionMode,
    uint32_t commandType) {

    LinearStream *commandStream = nullptr;
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    auto parentKernel = multiDispatchInfo.peekParentKernel();
    auto mainKernel = multiDispatchInfo.peekMainKernel();

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

    TimestampPacketHelper::programCsrDependencies<GfxFamily>(*commandStream, csrDependencies);

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

    size_t currentDispatchIndex = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        dispatchInfo.dispatchInitCommands(*commandStream);
        bool isMainKernel = (dispatchInfo.getKernel() == mainKernel);

        dispatchKernelCommands(commandQueue, dispatchInfo, commandType, *commandStream, isMainKernel,
                               currentDispatchIndex, currentTimestampPacketNodes, preemptionMode, interfaceDescriptorIndex,
                               offsetInterfaceDescriptorTable, *dsh, *ioh, *ssh);

        currentDispatchIndex++;
        dispatchInfo.dispatchEpilogueCommands(*commandStream);
    }
    if (mainKernel->requiresCacheFlushCommand(commandQueue)) {
        uint64_t postSyncAddress = 0;
        if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            auto timestampPacketNodeForPostSync = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
            postSyncAddress = timestampPacketNodeForPostSync->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
        }
        HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(commandStream, commandQueue, mainKernel, postSyncAddress);
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
    auto maxWorkGroupSize = static_cast<uint32_t>(commandQueue.getDevice().getDeviceInfo().maxWorkGroupSize);
    if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
        provideLocalWorkGroupSizeHints(commandQueue.getContextPtr(), maxWorkGroupSize, dispatchInfo);
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
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, nullptr, timestampPacketNode, TimestampPacketStorage::WriteOperationType::BeforeWalker);
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
            sshSize += HardwareCommandsHelper<GfxFamily>::getSizeRequiredForExecutionModel(IndirectHeap::SURFACE_STATE, *parentKernel);
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
        }
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
    }
}

} // namespace NEO
