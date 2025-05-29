/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/per_thread_data.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class CommandQueue;
class Device;
class Kernel;
class LinearStream;
class IndirectHeap;
enum PreemptionMode : uint32_t;
struct CrossThreadInfo;
struct MultiDispatchInfo;

template <typename GfxFamily>
struct HardwareCommandsHelperNoHeap : public PerThreadDataHelper {};

template <typename GfxFamily>
struct HardwareCommandsHelperWithHeap : public PerThreadDataHelper {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
};

template <typename GfxFamily>
using HardwareCommandsHelperBase = std::conditional_t<
    GfxFamily::isHeaplessRequired(),
    HardwareCommandsHelperNoHeap<GfxFamily>,
    HardwareCommandsHelperWithHeap<GfxFamily>>;

template <typename GfxFamily>
struct HardwareCommandsHelper : public HardwareCommandsHelperBase<GfxFamily> {
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    template <typename InterfaceDescryptorType>
    static InterfaceDescryptorType *getInterfaceDescriptor(
        const IndirectHeap &indirectHeap,
        uint64_t offsetInterfaceDescriptor,
        InterfaceDescryptorType *inlineInterfaceDescriptor);

    inline static uint32_t additionalSizeRequiredDsh();

    template <typename WalkerType, typename InterfaceDescriptorType>
    static size_t sendInterfaceDescriptorData(
        const IndirectHeap &indirectHeap,
        uint64_t offsetInterfaceDescriptor,
        uint64_t kernelStartOffset,
        size_t sizeCrossThreadData,
        size_t sizePerThreadData,
        size_t bindingTablePointer,
        [[maybe_unused]] size_t offsetSamplerState,
        uint32_t numSamplers,
        const uint32_t threadGroupCount,
        uint32_t numThreadsPerThreadGroup,
        const Kernel &kernel,
        uint32_t bindingTablePrefetchSize,
        PreemptionMode preemptionMode,
        const Device &device,
        WalkerType *walkerCmd,
        InterfaceDescriptorType *inlineInterfaceDescriptor);

    static void sendMediaStateFlush(
        LinearStream &commandStream,
        size_t offsetInterfaceDescriptorData);

    static void sendMediaInterfaceDescriptorLoad(
        LinearStream &commandStream,
        size_t offsetInterfaceDescriptorData,
        size_t sizeInterfaceDescriptorData);

    template <typename WalkerType>
    static size_t sendCrossThreadData(
        IndirectHeap &indirectHeap,
        Kernel &kernel,
        bool inlineDataProgrammingRequired,
        WalkerType *walkerCmd,
        uint32_t &sizeCrossThreadData,
        uint64_t scratchAddress,
        const RootDeviceEnvironment &rootDeviceEnvironment);

    template <typename WalkerType, typename InterfaceDescriptorType>
    static size_t sendIndirectState(
        LinearStream &commandStream,
        IndirectHeap &dsh,
        IndirectHeap &ioh,
        IndirectHeap &ssh,
        Kernel &kernel,
        uint64_t kernelStartOffset,
        uint32_t simd,
        const size_t localWorkSize[3],
        const uint32_t threadGroupCount,
        const uint64_t offsetInterfaceDescriptorTable,
        uint32_t &interfaceDescriptorIndex,
        PreemptionMode preemptionMode,
        WalkerType *walkerCmd,
        InterfaceDescriptorType *inlineInterfaceDescriptor,
        bool localIdsGenerationByRuntime,
        uint64_t scratchAddress,
        const Device &device);

    template <typename WalkerType>
    static void programInlineData(Kernel &kernel, WalkerType *walkerCmd, uint64_t indirectDataAddress, uint64_t scratchAddress);

    static void programPerThreadData(
        bool localIdsGenerationByRuntime,
        size_t &sizePerThreadData,
        size_t &sizePerThreadDataTotal,
        LinearStream &ioh,
        const Kernel &kernel,
        const size_t localWorkSize[3]);

    static size_t getSizeRequiredCS();

    static size_t getSizeRequiredDSH(
        const Kernel &kernel);
    static size_t getSizeRequiredIOH(
        const Kernel &kernel,
        const size_t localWorkSizes[3],
        const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeRequiredSSH(
        const Kernel &kernel);

    static size_t getTotalSizeRequiredDSH(
        const MultiDispatchInfo &multiDispatchInfo);
    static size_t getTotalSizeRequiredIOH(
        const MultiDispatchInfo &multiDispatchInfo);
    static size_t getTotalSizeRequiredSSH(
        const MultiDispatchInfo &multiDispatchInfo);

    template <typename WalkerType>
    static void setInterfaceDescriptorOffset(
        WalkerType *walkerCmd,
        uint32_t &interfaceDescriptorIndex);

    static bool kernelUsesLocalIds(const Kernel &kernel);
    static size_t checkForAdditionalBTAndSetBTPointer(IndirectHeap &ssh, const Kernel &kernel);
};
} // namespace NEO
