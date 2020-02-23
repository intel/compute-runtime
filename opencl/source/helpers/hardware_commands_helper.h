/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/indirect_heap/indirect_heap.h"
#include "opencl/source/built_ins/built_ins.h"
#include "opencl/source/helpers/per_thread_data.h"
#include "opencl/source/kernel/kernel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NEO {
class CommandQueue;
class LinearStream;
class IndirectHeap;
struct CrossThreadInfo;
struct MultiDispatchInfo;

template <typename GfxFamily>
using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

template <typename GfxFamily>
struct HardwareCommandsHelper : public PerThreadDataHelper {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    static uint32_t alignSlmSize(uint32_t slmSize);
    static uint32_t computeSlmValues(uint32_t slmSize);

    static INTERFACE_DESCRIPTOR_DATA *getInterfaceDescriptor(
        const IndirectHeap &indirectHeap,
        uint64_t offsetInterfaceDescriptor,
        INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

    static void setAdditionalInfo(
        INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor,
        const Kernel &kernel,
        const size_t &sizeCrossThreadData,
        const size_t &sizePerThreadData,
        const uint32_t threadsPerThreadGroup);

    inline static uint32_t additionalSizeRequiredDsh();

    static size_t sendInterfaceDescriptorData(
        const IndirectHeap &indirectHeap,
        uint64_t offsetInterfaceDescriptor,
        uint64_t kernelStartOffset,
        size_t sizeCrossThreadData,
        size_t sizePerThreadData,
        size_t bindingTablePointer,
        size_t offsetSamplerState,
        uint32_t numSamplers,
        uint32_t threadsPerThreadGroup,
        const Kernel &kernel,
        uint32_t bindingTablePrefetchSize,
        PreemptionMode preemptionMode,
        INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

    static void sendMediaStateFlush(
        LinearStream &commandStream,
        size_t offsetInterfaceDescriptorData);

    static void sendMediaInterfaceDescriptorLoad(
        LinearStream &commandStream,
        size_t offsetInterfaceDescriptorData,
        size_t sizeInterfaceDescriptorData);

    static size_t sendCrossThreadData(
        IndirectHeap &indirectHeap,
        Kernel &kernel,
        bool inlineDataProgrammingRequired,
        WALKER_TYPE<GfxFamily> *walkerCmd,
        uint32_t &sizeCrossThreadData);

    static size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, size_t bindingTableCount,
                                                   const void *srcKernelSsh, size_t srcKernelSshSize,
                                                   size_t numberOfBindingTableStates, size_t offsetOfBindingTable);

    static size_t sendIndirectState(
        LinearStream &commandStream,
        IndirectHeap &dsh,
        IndirectHeap &ioh,
        IndirectHeap &ssh,
        Kernel &kernel,
        uint64_t kernelStartOffset,
        uint32_t simd,
        const size_t localWorkSize[3],
        const uint64_t offsetInterfaceDescriptorTable,
        uint32_t &interfaceDescriptorIndex,
        PreemptionMode preemptionMode,
        WALKER_TYPE<GfxFamily> *walkerCmd,
        INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor,
        bool localIdsGenerationByRuntime);

    static void programPerThreadData(
        size_t &sizePerThreadData,
        const bool &localIdsGenerationByRuntime,
        LinearStream &ioh,
        uint32_t &simd,
        uint32_t &numChannels,
        const size_t localWorkSize[3],
        Kernel &kernel,
        size_t &sizePerThreadDataTotal,
        size_t &localWorkItems);

    static void updatePerThreadDataTotal(
        size_t &sizePerThreadData,
        uint32_t &simd,
        uint32_t &numChannels,
        size_t &sizePerThreadDataTotal,
        size_t &localWorkItems);

    inline static bool resetBindingTablePrefetch(Kernel &kernel);

    static size_t getSizeRequiredCS(const Kernel *kernel);
    static size_t getSizeRequiredForCacheFlush(const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress);
    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);
    static bool isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo);
    static size_t getSizeRequiredDSH(
        const Kernel &kernel);
    static size_t getSizeRequiredIOH(
        const Kernel &kernel,
        size_t localWorkSize = 256);
    static size_t getSizeRequiredSSH(
        const Kernel &kernel);

    static size_t getTotalSizeRequiredDSH(
        const MultiDispatchInfo &multiDispatchInfo);
    static size_t getTotalSizeRequiredIOH(
        const MultiDispatchInfo &multiDispatchInfo);
    static size_t getTotalSizeRequiredSSH(
        const MultiDispatchInfo &multiDispatchInfo);

    static size_t getSshSizeForExecutionModel(const Kernel &kernel);
    static void setInterfaceDescriptorOffset(
        WALKER_TYPE<GfxFamily> *walkerCmd,
        uint32_t &interfaceDescriptorIndex);

    static void programMiSemaphoreWait(LinearStream &commandStream,
                                       uint64_t compareAddress,
                                       uint32_t compareData,
                                       COMPARE_OPERATION compareMode);

    static MI_ATOMIC *programMiAtomic(LinearStream &commandStream, uint64_t writeAddress, typename MI_ATOMIC::ATOMIC_OPCODES opcode, typename MI_ATOMIC::DATA_SIZE dataSize);
    static void programMiAtomic(MI_ATOMIC &atomic, uint64_t writeAddress, typename MI_ATOMIC::ATOMIC_OPCODES opcode, typename MI_ATOMIC::DATA_SIZE dataSize);
    static void programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress);
    static void programBarrierEnable(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);
    static void adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo);

    static const size_t alignInterfaceDescriptorData = 64 * sizeof(uint8_t);
    static const uint32_t alignIndirectStatePointer = 64 * sizeof(uint8_t);

    static bool doBindingTablePrefetch();

    static bool inlineDataProgrammingRequired(const Kernel &kernel);
    static bool kernelUsesLocalIds(const Kernel &kernel);
};
} // namespace NEO
