/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/hardware_interface/hardware_interface.h"
#include "runtime/utilities/tag_allocator.h"

namespace OCLRT {

template <typename GfxFamily>
void HardwareInterface<GfxFamily>::dispatchWalker(
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    KernelOperation **blockedCommandsData,
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    TagNode<TimestampPacket> *previousTimestampPacketNode,
    TimestampPacket *currentTimestampPacket,
    PreemptionMode preemptionMode,
    bool blockQueue,
    uint32_t commandType) {

    OCLRT::LinearStream *commandStream = nullptr;
    OCLRT::IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    auto parentKernel = multiDispatchInfo.peekParentKernel();

    for (auto &dispatchInfo : multiDispatchInfo) {
        // Compute local workgroup sizes
        if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
            const auto lws = generateWorkgroupSize(dispatchInfo);
            const_cast<DispatchInfo &>(dispatchInfo).setLWS(lws);
        }
    }

    // Allocate command stream and indirect heaps
    if (blockQueue) {
        using KCH = KernelCommandsHelper<GfxFamily>;
        commandStream = new LinearStream(alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize),
                                         MemoryConstants::pageSize);
        if (parentKernel) {
            uint32_t colorCalcSize = commandQueue.getContext().getDefaultDeviceQueue()->colorCalcStateSize;

            commandQueue.allocateHeapMemory(
                IndirectHeap::DYNAMIC_STATE,
                commandQueue.getContext().getDefaultDeviceQueue()->getDshBuffer()->getUnderlyingBufferSize(),
                dsh);

            dsh->getSpace(colorCalcSize);
            ioh = dsh;
            commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE,
                                            KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<
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
                                                   UniqueIH(ssh), *commandQueue.getDevice().getMemoryManager());
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

    if (commandQueue.getDevice().getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        GpgpuWalkerHelper<GfxFamily>::dispatchOnDeviceWaitlistSemaphores(commandStream, commandQueue.getDevice(),
                                                                         numEventsInWaitList, eventWaitList);
        if (previousTimestampPacketNode) {
            auto compareAddress = previousTimestampPacketNode->tag->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd);
            KernelCommandsHelper<GfxFamily>::programMiSemaphoreWait(*commandStream, compareAddress, 1);
        }
    }

    dsh->align(KernelCommandsHelper<GfxFamily>::alignInterfaceDescriptorData);

    uint32_t interfaceDescriptorIndex = 0;
    const size_t offsetInterfaceDescriptorTable = dsh->getUsed();

    size_t totalInterfaceDescriptorTableSize = sizeof(INTERFACE_DESCRIPTOR_DATA);

    getDefaultDshSpace(offsetInterfaceDescriptorTable, commandQueue, multiDispatchInfo, totalInterfaceDescriptorTableSize,
                       parentKernel, dsh, commandStream);

    // Program media interface descriptor load
    KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    size_t currentDispatchIndex = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();

        DEBUG_BREAK_IF(!(dispatchInfo.getDim() >= 1 && dispatchInfo.getDim() <= 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getGWS().z == 1 || dispatchInfo.getDim() == 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getGWS().y == 1 || dispatchInfo.getDim() >= 2));
        DEBUG_BREAK_IF(!(dispatchInfo.getOffset().z == 0 || dispatchInfo.getDim() == 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getOffset().y == 0 || dispatchInfo.getDim() >= 2));

        // Determine SIMD size
        uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();

        // If we don't have a required WGS, compute one opportunistically
        auto maxWorkGroupSize = static_cast<uint32_t>(commandQueue.getDevice().getDeviceInfo().maxWorkGroupSize);
        if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
            provideLocalWorkGroupSizeHints(commandQueue.getContextPtr(), maxWorkGroupSize, dispatchInfo);
        }

        //Get dispatch geometry
        uint32_t dim = dispatchInfo.getDim();
        Vec3<size_t> gws = dispatchInfo.getGWS();
        Vec3<size_t> offset = dispatchInfo.getOffset();
        Vec3<size_t> swgs = dispatchInfo.getStartOfWorkgroups();

        // Compute local workgroup sizes
        Vec3<size_t> lws = dispatchInfo.getLocalWorkgroupSize();
        Vec3<size_t> elws = (dispatchInfo.getEnqueuedWorkgroupSize().x > 0) ? dispatchInfo.getEnqueuedWorkgroupSize() : lws;

        // Compute number of work groups
        Vec3<size_t> twgs = (dispatchInfo.getTotalNumberOfWorkgroups().x > 0) ? dispatchInfo.getTotalNumberOfWorkgroups()
                                                                              : generateWorkgroupsNumber(gws, lws);
        Vec3<size_t> nwgs = (dispatchInfo.getNumberOfWorkgroups().x > 0) ? dispatchInfo.getNumberOfWorkgroups() : twgs;

        // Patch our kernel constants
        *kernel.globalWorkOffsetX = static_cast<uint32_t>(offset.x);
        *kernel.globalWorkOffsetY = static_cast<uint32_t>(offset.y);
        *kernel.globalWorkOffsetZ = static_cast<uint32_t>(offset.z);

        *kernel.globalWorkSizeX = static_cast<uint32_t>(gws.x);
        *kernel.globalWorkSizeY = static_cast<uint32_t>(gws.y);
        *kernel.globalWorkSizeZ = static_cast<uint32_t>(gws.z);

        if ((&kernel == multiDispatchInfo.peekMainKernel()) || (kernel.localWorkSizeX2 == &Kernel::dummyPatchLocation)) {
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

        if (&kernel == multiDispatchInfo.peekMainKernel()) {
            *kernel.numWorkGroupsX = static_cast<uint32_t>(twgs.x);
            *kernel.numWorkGroupsY = static_cast<uint32_t>(twgs.y);
            *kernel.numWorkGroupsZ = static_cast<uint32_t>(twgs.z);
        }

        *kernel.workDim = dim;

        // Send our indirect object data
        size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

        dispatchProfilingPerfStartCommands(dispatchInfo, multiDispatchInfo, hwTimeStamps,
                                           hwPerfCounter, commandStream, commandQueue);

        dispatchWorkarounds(commandStream, commandQueue, kernel, true);

        bool setupTimestampPacket = currentTimestampPacket && (currentDispatchIndex == multiDispatchInfo.size() - 1);
        if (setupTimestampPacket) {
            GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(commandStream, nullptr, currentTimestampPacket,
                                                               TimestampPacket::WriteOperationType::BeforeWalker);
        }

        // Program the walker.  Invokes execution so all state should already be programmed
        auto pWalkerCmd = static_cast<WALKER_TYPE<GfxFamily> *>(commandStream->getSpace(sizeof(WALKER_TYPE<GfxFamily>)));
        *pWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

        if (setupTimestampPacket) {
            GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(commandStream, pWalkerCmd, currentTimestampPacket,
                                                               TimestampPacket::WriteOperationType::AfterWalker);
        }

        auto idd = obtainInterfaceDescriptorData(pWalkerCmd);

        auto offsetCrossThreadData = KernelCommandsHelper<GfxFamily>::sendIndirectState(
            *commandStream,
            *dsh,
            *ioh,
            *ssh,
            kernel,
            simd,
            localWorkSizes,
            offsetInterfaceDescriptorTable,
            interfaceDescriptorIndex,
            preemptionMode,
            idd);

        size_t globalOffsets[3] = {offset.x, offset.y, offset.z};
        size_t startWorkGroups[3] = {swgs.x, swgs.y, swgs.z};
        size_t numWorkGroups[3] = {nwgs.x, nwgs.y, nwgs.z};
        auto localWorkSize = GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(pWalkerCmd, globalOffsets, startWorkGroups,
                                                                                    numWorkGroups, localWorkSizes, simd);

        DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
        setOffsetCrossThreadData(pWalkerCmd, offsetCrossThreadData, interfaceDescriptorIndex);

        auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
        DEBUG_BREAK_IF(nullptr == threadPayload);

        auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
        auto localIdSizePerThread = PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels);
        localIdSizePerThread = std::max(localIdSizePerThread, sizeof(GRF));

        auto sizePerThreadDataTotal = getThreadsPerWG(simd, localWorkSize) * localIdSizePerThread;
        DEBUG_BREAK_IF(sizePerThreadDataTotal == 0); // Hardware requires at least 1 GRF of perThreadData for each thread in thread group

        auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
        auto IndirectDataLength = alignUp(static_cast<uint32_t>(sizeCrossThreadData + sizePerThreadDataTotal),
                                          WALKER_TYPE<GfxFamily>::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        pWalkerCmd->setIndirectDataLength(IndirectDataLength);

        dispatchWorkarounds(commandStream, commandQueue, kernel, false);
        currentDispatchIndex++;
    }
    dispatchProfilingPerfEndCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);
}

template <typename GfxFamily>
inline void BaseInterfaceVersion<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    OCLRT::Kernel *parentKernel,
    OCLRT::IndirectHeap *dsh,
    OCLRT::LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    if (!parentKernel) {
        dsh->getSpace(totalInterfaceDescriptorTableSize);
    } else {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }
}

template <typename GfxFamily>
inline typename BaseInterfaceVersion<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *
BaseInterfaceVersion<GfxFamily>::obtainInterfaceDescriptorData(
    WALKER_HANDLE pCmdData) {

    return nullptr;
}

template <typename GfxFamily>
inline void BaseInterfaceVersion<GfxFamily>::setOffsetCrossThreadData(
    WALKER_HANDLE pCmdData,
    size_t &offsetCrossThreadData,
    uint32_t &interfaceDescriptorIndex) {

    WALKER_TYPE<GfxFamily> *pCmd = static_cast<WALKER_TYPE<GfxFamily> *>(pCmdData);
    pCmd->setIndirectDataStartAddress(static_cast<uint32_t>(offsetCrossThreadData));
    pCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

template <typename GfxFamily>
inline void BaseInterfaceVersion<GfxFamily>::dispatchWorkarounds(
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue,
    OCLRT::Kernel &kernel,
    const bool &enable) {

    if (enable) {
        PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(commandStream, commandQueue.getDevice());
        // Implement enabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
    } else {
        // Implement disabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
        PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(commandStream, commandQueue.getDevice());
    }
}

template <typename GfxFamily>
inline void BaseInterfaceVersion<GfxFamily>::dispatchProfilingPerfStartCommands(
    const OCLRT::DispatchInfo &dispatchInfo,
    const MultiDispatchInfo &multiDispatchInfo,
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue) {

    if (&dispatchInfo == &*multiDispatchInfo.begin()) {
        // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
        if (hwTimeStamps != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream);
        }
        if (hwPerfCounter != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
        }
    }
}

template <typename GfxFamily>
inline void BaseInterfaceVersion<GfxFamily>::dispatchProfilingPerfEndCommands(
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream);
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

} // namespace OCLRT
