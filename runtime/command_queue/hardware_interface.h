/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/preemption_mode.h"

#include "CL/cl.h"

#include <cstdint>

namespace NEO {

class CommandQueue;
class DispatchInfo;
class IndirectHeap;
class Kernel;
class LinearStream;
struct HwPerfCounter;
struct HwTimeStamps;
struct KernelOperation;
struct MultiDispatchInfo;

template <class T>
struct TagNode;

template <typename GfxFamily>
using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

template <typename GfxFamily>
class HardwareInterface {
  public:
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

    static void dispatchWalker(
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        const CsrDependencies &csrDependencies,
        KernelOperation *blockedCommandsData,
        TagNode<HwTimeStamps> *hwTimeStamps,
        TagNode<HwPerfCounter> *hwPerfCounter,
        TimestampPacketDependencies *timestampPacketDependencies,
        TimestampPacketContainer *currentTimestampPacketNodes,
        uint32_t commandType);

    static void getDefaultDshSpace(
        const size_t &offsetInterfaceDescriptorTable,
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        size_t &totalInterfaceDescriptorTableSize,
        Kernel *parentKernel,
        IndirectHeap *dsh,
        LinearStream *commandStream);

    static void dispatchWorkarounds(
        LinearStream *commandStream,
        CommandQueue &commandQueue,
        Kernel &kernel,
        const bool &enable);

    static void dispatchProfilingPerfStartCommands(
        TagNode<HwTimeStamps> *hwTimeStamps,
        TagNode<HwPerfCounter> *hwPerfCounter,
        LinearStream *commandStream,
        CommandQueue &commandQueue);

    static void dispatchProfilingPerfEndCommands(
        TagNode<HwTimeStamps> *hwTimeStamps,
        TagNode<HwPerfCounter> *hwPerfCounter,
        LinearStream *commandStream,
        CommandQueue &commandQueue);

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
        Vec3<size_t> &numberOfWorkgroups,
        Vec3<size_t> &startOfWorkgroups);

    static WALKER_TYPE<GfxFamily> *allocateWalkerSpace(LinearStream &commandStream,
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
