/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"

#include "CL/cl.h"

#include <cstdint>

namespace NEO {

class CommandQueue;
class DispatchInfo;
class IndirectHeap;
class Kernel;
class LinearStream;
class HwPerfCounter;
class HwTimeStamps;
struct KernelOperation;
struct MultiDispatchInfo;

template <class T>
class TagNode;

template <typename GfxFamily>
class HardwareInterface {
  public:
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

    static void dispatchWalker(
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        const CsrDependencies &csrDependencies,
        KernelOperation *blockedCommandsData,
        TagNodeBase *hwTimeStamps,
        TagNodeBase *hwPerfCounter,
        TimestampPacketDependencies *timestampPacketDependencies,
        TimestampPacketContainer *currentTimestampPacketNodes,
        uint32_t commandType);

    static void getDefaultDshSpace(
        const size_t &offsetInterfaceDescriptorTable,
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        size_t &totalInterfaceDescriptorTableSize,
        IndirectHeap *dsh,
        LinearStream *commandStream);

    static void dispatchWorkarounds(
        LinearStream *commandStream,
        CommandQueue &commandQueue,
        Kernel &kernel,
        const bool &enable);

    static void dispatchProfilingPerfStartCommands(
        TagNodeBase *hwTimeStamps,
        TagNodeBase *hwPerfCounter,
        LinearStream *commandStream,
        CommandQueue &commandQueue);

    static void dispatchProfilingPerfEndCommands(
        TagNodeBase *hwTimeStamps,
        TagNodeBase *hwPerfCounter,
        LinearStream *commandStream,
        CommandQueue &commandQueue);

    static void dispatchDebugPauseCommands(
        LinearStream *commandStream,
        CommandQueue &commandQueue,
        DebugPauseState confirmationTrigger,
        DebugPauseState waitCondition,
        const HardwareInfo &hwInfo);

    static void programWalker(
        LinearStream &commandStream,
        Kernel &kernel,
        CommandQueue &commandQueue,
        TimestampPacketContainer *currentTimestampPacketNodes,
        IndirectHeap &dsh,
        IndirectHeap &ioh,
        IndirectHeap &ssh,
        size_t globalWorkSizes[3],
        size_t localWorkSizes[3],
        PreemptionMode preemptionMode,
        size_t currentDispatchIndex,
        uint32_t &interfaceDescriptorIndex,
        const DispatchInfo &dispatchInfo,
        size_t offsetInterfaceDescriptorTable,
        const Vec3<size_t> &numberOfWorkgroups,
        const Vec3<size_t> &startOfWorkgroups);

    static WALKER_TYPE *allocateWalkerSpace(LinearStream &commandStream,
                                            const Kernel &kernel);

    static void obtainIndirectHeaps(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo,
                                    bool blockedQueue, IndirectHeap *&dsh, IndirectHeap *&ioh, IndirectHeap *&ssh);

    static void dispatchKernelCommands(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, uint32_t commandType,
                                       LinearStream &commandStream, bool isMainKernel, size_t currentDispatchIndex,
                                       TimestampPacketContainer *currentTimestampPacketNodes, PreemptionMode preemptionMode,
                                       uint32_t &interfaceDescriptorIndex, size_t offsetInterfaceDescriptorTable,
                                       IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh);
};

} // namespace NEO
