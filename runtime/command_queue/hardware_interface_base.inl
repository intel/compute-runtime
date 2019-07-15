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
    KernelOperation **blockedCommandsData,
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
    TimestampPacketContainer *previousTimestampPacketNodes,
    TimestampPacketContainer *currentTimestampPacketNodes,
    PreemptionMode preemptionMode,
    bool blockQueue,
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
    if (blockQueue) {
        using KCH = HardwareCommandsHelper<GfxFamily>;

        constexpr static auto additionalAllocationSize = CSRequirements::csOverfetchSize;
        constexpr static auto allocationSize = MemoryConstants::pageSize64k - additionalAllocationSize;
        commandStream = new LinearStream();
        commandQueue.getGpgpuCommandStreamReceiver().ensureCommandBufferAllocation(*commandStream, allocationSize, additionalAllocationSize);

        if (parentKernel) {
            uint32_t colorCalcSize = commandQueue.getContext().getDefaultDeviceQueue()->colorCalcStateSize;

            commandQueue.allocateHeapMemory(
                IndirectHeap::DYNAMIC_STATE,
                commandQueue.getContext().getDefaultDeviceQueue()->getDshBuffer()->getUnderlyingBufferSize(),
                dsh);

            dsh->getSpace(colorCalcSize);
            ioh = dsh;
            commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE,
                                            HardwareCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<
                                                IndirectHeap::SURFACE_STATE>(*parentKernel) +
                                                KCH::getTotalSizeRequiredSSH(multiDispatchInfo),
                                            ssh);
        } else {
            commandQueue.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, KCH::getTotalSizeRequiredDSH(multiDispatchInfo), dsh);
            commandQueue.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, KCH::getTotalSizeRequiredIOH(multiDispatchInfo), ioh);
            commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE, KCH::getTotalSizeRequiredSSH(multiDispatchInfo), ssh);
        }

        using UniqueIH = std::unique_ptr<IndirectHeap>;
        *blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(commandStream), UniqueIH(dsh), UniqueIH(ioh),
                                                   UniqueIH(ssh), *commandQueue.getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
        if (parentKernel) {
            (*blockedCommandsData)->doNotFreeISH = true;
        }
    } else {
        commandStream = &commandQueue.getCS(0);
        if (parentKernel && (commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0).getUsed() > 0)) {
            commandQueue.releaseIndirectHeap(IndirectHeap::SURFACE_STATE);
        }
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
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

        if ((&kernel == mainKernel) || (kernel.localWorkSizeX2 == &Kernel::dummyPatchLocation)) {
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

        if (&kernel == mainKernel) {
            *kernel.numWorkGroupsX = static_cast<uint32_t>(totalNumberOfWorkgroups.x);
            *kernel.numWorkGroupsY = static_cast<uint32_t>(totalNumberOfWorkgroups.y);
            *kernel.numWorkGroupsZ = static_cast<uint32_t>(totalNumberOfWorkgroups.z);
        }

        *kernel.workDim = dim;

        // Send our indirect object data
        size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

        dispatchWorkarounds(commandStream, commandQueue, kernel, true);

        if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            auto timestampPacketNode = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
            GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(commandStream, nullptr, timestampPacketNode, TimestampPacketStorage::WriteOperationType::BeforeWalker);
        }

        programWalker(*commandStream, kernel, commandQueue, currentTimestampPacketNodes, *dsh, *ioh, *ssh, globalWorkSizes,
                      localWorkSizes, preemptionMode, currentDispatchIndex, interfaceDescriptorIndex, dispatchInfo,
                      offsetInterfaceDescriptorTable, numberOfWorkgroups, startOfWorkgroups);

        dispatchWorkarounds(commandStream, commandQueue, kernel, false);

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

} // namespace NEO
