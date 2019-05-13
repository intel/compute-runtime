/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/per_thread_data.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/scheduler/scheduler_kernel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NEO {

class LinearStream;
class IndirectHeap;
struct CrossThreadInfo;
struct MultiDispatchInfo;

template <typename GfxFamily>
using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

template <typename GfxFamily>
struct KernelCommandsHelper : public PerThreadDataHelper {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;

    static uint32_t computeSlmValues(uint32_t valueIn);

    static INTERFACE_DESCRIPTOR_DATA *getInterfaceDescriptor(
        const IndirectHeap &indirectHeap,
        uint64_t offsetInterfaceDescriptor,
        INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

    static void setAdditionalInfo(
        INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor,
        const size_t &sizeCrossThreadData,
        const size_t &sizePerThreadData);

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
        uint32_t sizeSlm,
        uint32_t bindingTablePrefetchSize,
        bool barrierEnable,
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

    static size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, const KernelInfo &srcKernelInfo,
                                                   const void *srcKernelSsh, size_t srcKernelSshSize,
                                                   size_t numberOfBindingTableStates, size_t offsetOfBindingTable);

    static size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, const KernelInfo &srcKernelInfo) {
        return pushBindingTableAndSurfaceStates(dstHeap, srcKernelInfo, srcKernelInfo.heapInfo.pSsh,
                                                srcKernelInfo.heapInfo.pKernelHeader->SurfaceStateHeapSize,
                                                (srcKernelInfo.patchInfo.bindingTableState != nullptr) ? srcKernelInfo.patchInfo.bindingTableState->Count : 0,
                                                (srcKernelInfo.patchInfo.bindingTableState != nullptr) ? srcKernelInfo.patchInfo.bindingTableState->Offset : 0);
    }

    static size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, const Kernel &srcKernel) {
        return pushBindingTableAndSurfaceStates(dstHeap, srcKernel.getKernelInfo(),
                                                srcKernel.getSurfaceStateHeap(), srcKernel.getSurfaceStateHeapSize(),
                                                srcKernel.getNumberOfBindingTableStates(), srcKernel.getBindingTableOffset());
    }

    static size_t sendIndirectState(
        LinearStream &commandStream,
        IndirectHeap &dsh,
        IndirectHeap &ioh,
        IndirectHeap &ssh,
        Kernel &kernel,
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

    static void setKernelStartOffset(
        uint64_t &kernelStartOffset,
        bool kernelAllocation,
        const KernelInfo &kernelInfo,
        const bool &localIdsGenerationByRuntime,
        const bool &kernelUsesLocalIds,
        Kernel &kernel);

    static size_t getSizeRequiredCS(const Kernel *kernel);
    static size_t getSizeRequiredForCacheFlush(const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress, uint64_t postSyncData);
    static bool isPipeControlWArequired();
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

    template <IndirectHeap::Type heapType>
    static size_t getSizeRequiredForExecutionModel(const Kernel &kernel) {
        typedef typename GfxFamily::BINDING_TABLE_STATE BINDING_TABLE_STATE;

        size_t totalSize = 0;
        BlockKernelManager *blockManager = kernel.getProgram()->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());
        uint32_t maxBindingTableCount = 0;

        if (heapType == IndirectHeap::SURFACE_STATE) {
            totalSize = BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE - 1;

            for (uint32_t i = 0; i < blockCount; i++) {
                const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
                totalSize += pBlockInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize;
                totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

                maxBindingTableCount = std::max(maxBindingTableCount, pBlockInfo->patchInfo.bindingTableState->Count);
            }
        }

        if (heapType == IndirectHeap::INDIRECT_OBJECT || heapType == IndirectHeap::SURFACE_STATE) {
            BuiltIns &builtIns = *kernel.getDevice().getExecutionEnvironment()->getBuiltIns();
            SchedulerKernel &scheduler = builtIns.getSchedulerKernel(kernel.getContext());

            if (heapType == IndirectHeap::INDIRECT_OBJECT) {
                totalSize += getSizeRequiredIOH(scheduler);
            } else {
                totalSize += getSizeRequiredSSH(scheduler);

                totalSize += maxBindingTableCount * sizeof(BINDING_TABLE_STATE) * DeviceQueue::interfaceDescriptorEntries;
                totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            }
        }
        return totalSize;
    }

    static void setInterfaceDescriptorOffset(
        WALKER_TYPE<GfxFamily> *walkerCmd,
        uint32_t &interfaceDescriptorIndex);

    static void programMiSemaphoreWait(LinearStream &commandStream, uint64_t compareAddress, uint32_t compareData);
    static MI_ATOMIC *programMiAtomic(LinearStream &commandStream, uint64_t writeAddress, typename MI_ATOMIC::ATOMIC_OPCODES opcode, typename MI_ATOMIC::DATA_SIZE dataSize);
    static void programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress, uint64_t postSyncData);

    static const size_t alignInterfaceDescriptorData = 64 * sizeof(uint8_t);
    static const uint32_t alignIndirectStatePointer = 64 * sizeof(uint8_t);

    static bool doBindingTablePrefetch();

    static bool isRuntimeLocalIdsGenerationRequired(uint32_t workDim, size_t *gws, size_t *lws);
    static bool inlineDataProgrammingRequired(const Kernel &kernel);
    static bool kernelUsesLocalIds(const Kernel &kernel);
};
} // namespace NEO
