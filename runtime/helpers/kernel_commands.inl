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

#include "runtime/command_queue/local_id_gen.h"
#include "runtime/command_stream/csr_definitions.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/address_patch.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
uint32_t KernelCommandsHelper<GfxFamily>::computeSlmValues(uint32_t valueIn) {
    auto value = std::max(valueIn, 1024u);
    value = Math::nextPowerOfTwo(value);
    value = Math::getMinLsbSet(value);
    value = value - 9;
    DEBUG_BREAK_IF(value > 7);
    return value * !!valueIn;
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() {
    return 2 * sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
           sizeof(typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredDSH(
    const Kernel &kernel) {
    typedef typename GfxFamily::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename GfxFamily::SAMPLER_STATE SAMPLER_STATE;
    const auto &patchInfo = kernel.getKernelInfo().patchInfo;
    auto samplerCount = patchInfo.samplerStateArray
                            ? patchInfo.samplerStateArray->Count
                            : 0;
    auto totalSize = samplerCount
                         ? alignUp(samplerCount * sizeof(SAMPLER_STATE), INTERFACE_DESCRIPTOR_DATA::SAMPLERSTATEPOINTER_ALIGN_SIZE)
                         : 0;

    auto borderColorSize = patchInfo.samplerStateArray
                               ? patchInfo.samplerStateArray->Offset - patchInfo.samplerStateArray->BorderColorOffset
                               : 0;

    borderColorSize = alignUp(borderColorSize + alignIndirectStatePointer - 1, alignIndirectStatePointer);

    totalSize += sizeof(INTERFACE_DESCRIPTOR_DATA) + borderColorSize;

    DEBUG_BREAK_IF(!(totalSize >= kernel.getDynamicStateHeapSize() || kernel.getKernelInfo().isVmeWorkload));

    return alignUp(totalSize, alignInterfaceDescriptorData);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredIOH(
    const Kernel &kernel,
    size_t localWorkSize) {
    typedef typename GfxFamily::GPGPU_WALKER GPGPU_WALKER;

    auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
    DEBUG_BREAK_IF(nullptr == threadPayload);

    auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
    return alignUp((kernel.getCrossThreadDataSize() +
                    getPerThreadDataSizeTotal(kernel.getKernelInfo().getMaxSimdSize(), numChannels, localWorkSize)),
                   GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredSSH(
    const Kernel &kernel) {
    typedef typename GfxFamily::BINDING_TABLE_STATE BINDING_TABLE_STATE;
    auto sizeSSH = kernel.getSurfaceStateHeapSize();
    sizeSSH += sizeSSH ? BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE : 0;
    return sizeSSH;
}

template <typename SizeGetterT, typename... ArgsT>
size_t getSizeRequired(const MultiDispatchInfo &multiDispatchInfo, SizeGetterT &&getSize, ArgsT... args) {
    size_t totalSize = 0;
    auto it = multiDispatchInfo.begin();
    for (auto e = multiDispatchInfo.end(); it != e; ++it) {
        totalSize = alignUp(totalSize, MemoryConstants::pageSize);
        totalSize += getSize(*it, std::forward<ArgsT>(args)...);
    }
    return totalSize;
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredDSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredIOH(*dispatchInfo.getKernel(), Math::computeTotalElementsCount(dispatchInfo.getLocalWorkgroupSize())); });
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredSSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
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
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    // Allocate some memory for the interface descriptor
    auto pInterfaceDescriptor = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(indirectHeap.getCpuBase(), (size_t)offsetInterfaceDescriptor));
    *pInterfaceDescriptor = GfxFamily::cmdInitInterfaceDescriptorData;

    // Program the kernel start pointer
    pInterfaceDescriptor->setKernelStartPointerHigh(kernelStartOffset >> 32);
    pInterfaceDescriptor->setKernelStartPointer((uint32_t)kernelStartOffset);

    // # of threads in thread group should be based on LWS.
    pInterfaceDescriptor->setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    DEBUG_BREAK_IF((sizeCrossThreadData % sizeof(GRF)) != 0);
    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / sizeof(GRF));
    DEBUG_BREAK_IF(numGrfCrossThreadData == 0);
    pInterfaceDescriptor->setCrossThreadConstantDataReadLength(numGrfCrossThreadData);
    pInterfaceDescriptor->setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);

    DEBUG_BREAK_IF((sizePerThreadData % sizeof(GRF)) != 0);
    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(GRF));

    // at least 1 GRF of perThreadData for each thread in a thread group when sizeCrossThreadData != 0
    numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);
    pInterfaceDescriptor->setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);

    pInterfaceDescriptor->setBindingTablePointer(static_cast<uint32_t>(bindingTablePointer));

    pInterfaceDescriptor->setSamplerStatePointer(static_cast<uint32_t>(offsetSamplerState));

    DEBUG_BREAK_IF(numSamplers > 16);
    auto samplerCountState = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((numSamplers + 3) / 4);
    pInterfaceDescriptor->setSamplerCount(samplerCountState);

    auto programmableIDSLMSize = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(computeSlmValues(sizeSlm));

    pInterfaceDescriptor->setSharedLocalMemorySize(programmableIDSLMSize);
    pInterfaceDescriptor->setBarrierEnable(barrierEnable);

    pInterfaceDescriptor->setBindingTableEntryCount(bindingTablePrefetchSize);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(pInterfaceDescriptor, preemptionMode);

    return (size_t)offsetInterfaceDescriptor;
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::sendMediaStateFlush(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData) {

    typedef typename GfxFamily::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;
    auto pCmd = (MEDIA_STATE_FLUSH *)commandStream.getSpace(sizeof(MEDIA_STATE_FLUSH));
    *pCmd = GfxFamily::cmdInitMediaStateFlush;
    pCmd->setInterfaceDescriptorOffset((uint32_t)offsetInterfaceDescriptorData);
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData,
    size_t sizeInterfaceDescriptorData) {
    {
        typedef typename GfxFamily::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;
        auto pCmd = (MEDIA_STATE_FLUSH *)commandStream.getSpace(sizeof(MEDIA_STATE_FLUSH));
        *pCmd = GfxFamily::cmdInitMediaStateFlush;
    }

    {
        typedef typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
        auto pCmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)commandStream.getSpace(sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD));
        *pCmd = GfxFamily::cmdInitMediaInterfaceDescriptorLoad;
        pCmd->setInterfaceDescriptorDataStartAddress((uint32_t)offsetInterfaceDescriptorData);
        pCmd->setInterfaceDescriptorTotalLength((uint32_t)sizeInterfaceDescriptorData);
    }
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel) {
    typedef typename GfxFamily::GPGPU_WALKER GPGPU_WALKER;

    indirectHeap.align(GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

    auto offsetCrossThreadData = indirectHeap.getUsed();
    auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
    char *pDest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
    memcpy_s(pDest, sizeCrossThreadData, kernel.getCrossThreadData(), sizeCrossThreadData);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(indirectHeap.getHeapGpuStartOffset());
}

// Returned binding table pointer is relative to given heap (which is assumed to be the Surface state base addess)
// as required by the INTERFACE_DESCRIPTOR_DATA.
template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, const KernelInfo &srcKernelInfo,
                                                                         const void *srcKernelSsh, size_t srcKernelSshSize,
                                                                         size_t numberOfBindingTableStates, size_t offsetOfBindingTable) {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    if ((srcKernelInfo.patchInfo.bindingTableState == nullptr) || (srcKernelInfo.patchInfo.bindingTableState->Count == 0)) {
        // according to compiler, kernel does not reference BTIs to stateful surfaces, so there's nothing to patch
        return 0;
    }
    size_t sshSize = srcKernelSshSize;
    DEBUG_BREAK_IF(srcKernelSsh == nullptr);

    auto srcSurfaceState = srcKernelSsh;
    // Align the heap and allocate space for new ssh data
    dstHeap.align(BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
    auto dstSurfaceState = dstHeap.getSpace(sshSize);

    // Compiler sends BTI table that is already populated with surface state pointers relative to local SSH.
    // We may need to patch these pointers so that they are relative to surface state base address
    if (dstSurfaceState == dstHeap.getCpuBase()) {
        // nothing to patch, we're at the start of heap (which is assumed to be the surface state base address)
        // we need to simply copy the ssh (including BTIs from compiler)
        memcpy_s(dstSurfaceState, sshSize, srcSurfaceState, sshSize);
        return offsetOfBindingTable;
    }

    // We can copy-over the surface states, but BTIs will need to be patched
    memcpy_s(dstSurfaceState, sshSize, srcSurfaceState, offsetOfBindingTable);

    uint32_t surfaceStatesOffset = static_cast<uint32_t>(ptrDiff(dstSurfaceState, dstHeap.getCpuBase()));

    // march over BTIs and offset the pointers based on surface state base address
    auto *dstBtiTableBase = reinterpret_cast<BINDING_TABLE_STATE *>(ptrOffset(dstSurfaceState, offsetOfBindingTable));
    DEBUG_BREAK_IF(reinterpret_cast<uintptr_t>(dstBtiTableBase) % INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER_ALIGN_SIZE != 0);
    auto *srcBtiTableBase = reinterpret_cast<const BINDING_TABLE_STATE *>(ptrOffset(srcSurfaceState, offsetOfBindingTable));
    BINDING_TABLE_STATE bti;
    bti.init(); // init whole DWORD - i.e. not just the SurfaceStatePointer bits
    for (uint32_t i = 0, e = (uint32_t)numberOfBindingTableStates; i != e; ++i) {
        uint32_t localSurfaceStateOffset = srcBtiTableBase[i].getSurfaceStatePointer();
        uint32_t offsetedSurfaceStateOffset = localSurfaceStateOffset + surfaceStatesOffset;
        bti.setSurfaceStatePointer(offsetedSurfaceStateOffset); // patch just the SurfaceStatePointer bits
        dstBtiTableBase[i] = bti;
        DEBUG_BREAK_IF(bti.getRawData(0) % sizeof(BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE) != 0);
    }

    return ptrDiff(dstBtiTableBase, dstHeap.getCpuBase());
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::sendIndirectState(
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
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    DEBUG_BREAK_IF(simd != 8 && simd != 16 && simd != 32);

    // Copy the kernel over to the ISH
    auto kernelStartOffset = 0llu;
    const auto &kernelInfo = kernel.getKernelInfo();
    auto kernelAllocation = kernelInfo.getGraphicsAllocation();
    DEBUG_BREAK_IF(!kernelAllocation);
    if (kernelAllocation) {
        kernelStartOffset = kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();
    }
    kernelStartOffset += kernel.getStartOffset();
    const auto &patchInfo = kernelInfo.patchInfo;

    auto dstBindingTablePointer = pushBindingTableAndSurfaceStates(ssh, kernel);

    // Copy our sampler state if it exists
    size_t samplerStateOffset = 0;
    uint32_t samplerCount = 0;
    if (patchInfo.samplerStateArray) {
        size_t borderColorOffset = 0;
        samplerCount = patchInfo.samplerStateArray->Count;
        auto sizeSamplerState = sizeof(SAMPLER_STATE) * samplerCount;
        auto borderColorSize = patchInfo.samplerStateArray->Offset - patchInfo.samplerStateArray->BorderColorOffset;

        dsh.align(alignIndirectStatePointer);
        borderColorOffset = dsh.getUsed();

        auto borderColor = dsh.getSpace(borderColorSize);

        memcpy_s(borderColor, borderColorSize,
                 ptrOffset(kernel.getDynamicStateHeap(), patchInfo.samplerStateArray->BorderColorOffset),
                 borderColorSize);

        dsh.align(INTERFACE_DESCRIPTOR_DATA::SAMPLERSTATEPOINTER_ALIGN_SIZE);
        samplerStateOffset = dsh.getUsed();

        auto samplerState = dsh.getSpace(sizeSamplerState);

        memcpy_s(samplerState, sizeSamplerState,
                 ptrOffset(kernel.getDynamicStateHeap(), patchInfo.samplerStateArray->Offset),
                 sizeSamplerState);

        auto pSmplr = (SAMPLER_STATE *)(samplerState);
        for (uint32_t i = 0; i < samplerCount; i++) {
            pSmplr->setIndirectStatePointer((uint32_t)borderColorOffset);
            pSmplr++;
        }
    }

    // Send thread data
    auto offsetCrossThreadData = sendCrossThreadData(
        ioh,
        kernel);

    auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
    DEBUG_BREAK_IF(nullptr == threadPayload);

    auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
    sendPerThreadData(
        ioh,
        simd,
        numChannels,
        localWorkSize,
        kernel.getKernelInfo().workgroupDimensionsOrder,
        kernel.usesOnlyImages());

    // send interface descriptor data
    auto localWorkItems = localWorkSize[0] * localWorkSize[1] * localWorkSize[2];
    auto sizePerThreadData = getPerThreadSizeLocalIDs(simd, numChannels);
    auto threadsPerThreadGroup = static_cast<uint32_t>(getThreadsPerWG(simd, localWorkItems));

    uint64_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable + interfaceDescriptorIndex * sizeof(INTERFACE_DESCRIPTOR_DATA);

    DEBUG_BREAK_IF(patchInfo.executionEnvironment == nullptr);

    auto bindingTablePrefetchSize = std::min(31u, static_cast<uint32_t>(kernel.getNumberOfBindingTableStates()));
    if (kernel.isSchedulerKernel) {
        bindingTablePrefetchSize = 0;
    }

    KernelCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
        dsh,
        offsetInterfaceDescriptor,
        kernelStartOffset,
        kernel.getCrossThreadDataSize(),
        sizePerThreadData,
        dstBindingTablePointer,
        samplerStateOffset,
        samplerCount,
        threadsPerThreadGroup,
        kernel.slmTotalSize,
        bindingTablePrefetchSize,
        !!patchInfo.executionEnvironment->HasBarriers,
        preemptionMode,
        inlineInterfaceDescriptor);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        PatchInfoData patchInfoData(kernelStartOffset, 0, PatchInfoAllocationType::InstructionHeap, dsh.getGraphicsAllocation()->getGpuAddress(), offsetInterfaceDescriptor, PatchInfoAllocationType::DynamicStateHeap);
        kernel.getPatchInfoDataList().push_back(patchInfoData);
    }

    // Program media state flush to set interface descriptor offset
    KernelCommandsHelper<GfxFamily>::sendMediaStateFlush(
        commandStream,
        interfaceDescriptorIndex);

    return offsetCrossThreadData;
}
} // namespace OCLRT
