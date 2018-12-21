/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include <cstdint>

namespace OCLRT {

class CommandQueue;
class DispatchInfo;
class IndirectHeap;
class Kernel;
class LinearStream;
class TimestampPacket;
enum class PreemptionMode : uint32_t;
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
        cl_uint numEventsInWaitList,
        const cl_event *eventWaitList,
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

    static INTERFACE_DESCRIPTOR_DATA *obtainInterfaceDescriptorData(
        WALKER_TYPE<GfxFamily> *walkerCmd);

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

    static WALKER_TYPE<GfxFamily> *allocateWalkerSpace(LinearStream &commandStream,
                                                       const Kernel &kernel);
};

} // namespace OCLRT
