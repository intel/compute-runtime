/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/per_thread_data.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace OCLRT {

class LinearStream;
class IndirectHeap;
struct CrossThreadInfo;
struct MultiDispatchInfo;

template <typename GfxFamily>
struct KernelCommandsHelper : public PerThreadDataHelper {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

    static uint32_t computeSlmValues(uint32_t valueIn);

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
        Kernel &kernel);

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
        const uint32_t interfaceDescriptorIndex,
        PreemptionMode preemptionMode,
        INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

    static size_t getSizeRequiredCS();
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
            BuiltIns &builtIns = kernel.getDevice().getBuiltIns();
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

    static const size_t alignInterfaceDescriptorData = 64 * sizeof(uint8_t);
    static const uint32_t alignIndirectStatePointer = 64 * sizeof(uint8_t);
};
} // namespace OCLRT
