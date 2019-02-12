/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include "runtime/command_stream/preemption_mode.h"
#include <cstdint>

namespace OCLRT {

class CommandQueue;
class DispatchInfo;
class IndirectHeap;
class Kernel;
class LinearStream;
class TimestampPacket;
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
        KernelOperation **blockedCommandsData,
        TagNode<HwTimeStamps> *hwTimeStamps,
        HwPerfCounter *hwPerfCounter,
        TimestampPacketContainer *previousTimestampPacketNodes,
        TimestampPacketContainer *currentTimestampPacketNodes,
        PreemptionMode preemptionMode,
        bool blockQueue,
        uint32_t commandType = 0);

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
        const DispatchInfo &dispatchInfo,
        const MultiDispatchInfo &multiDispatchInfo,
        TagNode<HwTimeStamps> *hwTimeStamps,
        HwPerfCounter *hwPerfCounter,
        LinearStream *commandStream,
        CommandQueue &commandQueue);

    static void dispatchProfilingPerfEndCommands(
        TagNode<HwTimeStamps> *hwTimeStamps,
        HwPerfCounter *hwPerfCounter,
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
};

} // namespace OCLRT
