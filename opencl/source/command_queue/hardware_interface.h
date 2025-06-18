/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/vec.h"

#include <cstdint>

namespace NEO {

class CommandQueue;
class CsrDependencies;
class DispatchInfo;
class Event;
class IndirectHeap;
class Kernel;
class LinearStream;
class HwPerfCounter;
class HwTimeStamps;
class TagNodeBase;
class TimestampPacketContainer;
struct KernelOperation;
struct MultiDispatchInfo;
struct TimestampPacketDependencies;
struct KernelInfo;
struct EncodeWalkerArgs;
struct HardwareInfo;

template <class T>
class TagNode;

struct HardwareInterfaceWalkerArgs {
    size_t globalWorkSizes[3] = {};
    size_t localWorkSizes[3] = {};
    TagNodeBase *hwTimeStamps = nullptr;
    TagNodeBase *hwPerfCounter = nullptr;
    TagNodeBase *multiRootDeviceEventStamp = nullptr;
    TimestampPacketDependencies *timestampPacketDependencies = nullptr;
    TimestampPacketContainer *currentTimestampPacketNodes = nullptr;
    const Vec3<size_t> *numberOfWorkgroups = nullptr;
    const Vec3<size_t> *startOfWorkgroups = nullptr;
    KernelOperation *blockedCommandsData = nullptr;
    Event *event = nullptr;
    size_t currentDispatchIndex = 0;
    size_t offsetInterfaceDescriptorTable = 0;
    PreemptionMode preemptionMode = PreemptionMode::Initial;
    uint32_t commandType = 0;
    uint32_t interfaceDescriptorIndex = 0;
    bool isMainKernel = false;
    bool relaxedOrderingEnabled = false;
    bool blocking = false;
};

struct HardwareInterfaceHelper {
    static void setEncodeWalkerArgsExt(
        EncodeWalkerArgs &encodeWalkerArgs,
        const KernelInfo &kernelInfo);
};

template <typename GfxFamily>
class HardwareInterface {
  public:
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    template <typename WalkerType>
    static void dispatchWalker(
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        const CsrDependencies &csrDependencies,
        HardwareInterfaceWalkerArgs &walkerArgs);

    static void dispatchWalkerCommon(
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        const CsrDependencies &csrDependencies,
        HardwareInterfaceWalkerArgs &walkerArgs);

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

    template <typename WalkerType>
    static void programWalker(
        LinearStream &commandStream,
        Kernel &kernel,
        CommandQueue &commandQueue,
        IndirectHeap &dsh,
        IndirectHeap &ioh,
        IndirectHeap &ssh,
        const DispatchInfo &dispatchInfo,
        HardwareInterfaceWalkerArgs &walkerArgs);

    template <typename WalkerType>
    static WalkerType *allocateWalkerSpace(LinearStream &commandStream,
                                           const Kernel &kernel);

    static void obtainIndirectHeaps(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo,
                                    bool blockedQueue, IndirectHeap *&dsh, IndirectHeap *&ioh, IndirectHeap *&ssh);

    template <typename WalkerType>
    static void dispatchKernelCommands(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream,
                                       IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh,
                                       HardwareInterfaceWalkerArgs &walkerArgs);
};

} // namespace NEO
