/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/csr_definitions.h"
#include "core/command_stream/preemption.h"
#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/address_patch.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/basic_math.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/string.h"
#include "core/indirect_heap/indirect_heap.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/block_kernel_manager.h"
#include "runtime/scheduler/scheduler_kernel.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) { return false; }

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 1024u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    UNRECOVERABLE_IF(slmSize > 64u * KB);
    return slmSize;
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::computeSlmValues(uint32_t slmSize) {
    auto value = std::max(slmSize, 1024u);
    value = Math::nextPowerOfTwo(value);
    value = Math::getMinLsbSet(value);
    value = value - 9;
    DEBUG_BREAK_IF(value > 7);
    return value * !!slmSize;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredDSH(
    const Kernel &kernel) {
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
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

    totalSize += borderColorSize + additionalSizeRequiredDsh();

    DEBUG_BREAK_IF(!(totalSize >= kernel.getDynamicStateHeapSize() || kernel.getKernelInfo().isVmeWorkload));

    return alignUp(totalSize, alignInterfaceDescriptorData);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredIOH(
    const Kernel &kernel,
    size_t localWorkSize) {
    typedef typename GfxFamily::WALKER_TYPE WALKER_TYPE;

    auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
    DEBUG_BREAK_IF(nullptr == threadPayload);

    auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
    uint32_t grfSize = sizeof(typename GfxFamily::GRF);
    return alignUp((kernel.getCrossThreadDataSize() +
                    getPerThreadDataSizeTotal(kernel.getKernelInfo().getMaxSimdSize(), grfSize, numChannels, localWorkSize)),
                   WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredSSH(
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
        totalSize = alignUp(totalSize, MemoryConstants::cacheLineSize);
        totalSize += getSize(*it, std::forward<ArgsT>(args)...);
    }
    totalSize = alignUp(totalSize, MemoryConstants::pageSize);
    return totalSize;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredDSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredIOH(*dispatchInfo.getKernel(), Math::computeTotalElementsCount(dispatchInfo.getLocalWorkgroupSize())); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredSSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSshSizeForExecutionModel(const Kernel &kernel) {
    typedef typename GfxFamily::BINDING_TABLE_STATE BINDING_TABLE_STATE;

    size_t totalSize = 0;
    BlockKernelManager *blockManager = kernel.getProgram()->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());
    uint32_t maxBindingTableCount = 0;

    totalSize = BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE - 1;

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);
        totalSize += pBlockInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize;
        totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

        maxBindingTableCount = std::max(maxBindingTableCount, pBlockInfo->patchInfo.bindingTableState->Count);
    }

    BuiltIns &builtIns = *kernel.getDevice().getExecutionEnvironment()->getBuiltIns();
    SchedulerKernel &scheduler = builtIns.getSchedulerKernel(kernel.getContext());

    totalSize += getSizeRequiredSSH(scheduler);

    totalSize += maxBindingTableCount * sizeof(BINDING_TABLE_STATE) * DeviceQueue::interfaceDescriptorEntries;
    totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

    return totalSize;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
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
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    // Allocate some memory for the interface descriptor
    auto pInterfaceDescriptor = getInterfaceDescriptor(indirectHeap, offsetInterfaceDescriptor, inlineInterfaceDescriptor);
    *pInterfaceDescriptor = GfxFamily::cmdInitInterfaceDescriptorData;

    // Program the kernel start pointer
    pInterfaceDescriptor->setKernelStartPointerHigh(kernelStartOffset >> 32);
    pInterfaceDescriptor->setKernelStartPointer((uint32_t)kernelStartOffset);

    // # of threads in thread group should be based on LWS.
    pInterfaceDescriptor->setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    pInterfaceDescriptor->setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);

    setAdditionalInfo(pInterfaceDescriptor, kernel, sizeCrossThreadData, sizePerThreadData, threadsPerThreadGroup);

    pInterfaceDescriptor->setBindingTablePointer(static_cast<uint32_t>(bindingTablePointer));

    pInterfaceDescriptor->setSamplerStatePointer(static_cast<uint32_t>(offsetSamplerState));

    DEBUG_BREAK_IF(numSamplers > 16);
    auto samplerCountState = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((numSamplers + 3) / 4);
    pInterfaceDescriptor->setSamplerCount(samplerCountState);

    auto programmableIDSLMSize = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(computeSlmValues(kernel.slmTotalSize));

    pInterfaceDescriptor->setSharedLocalMemorySize(programmableIDSLMSize);
    programBarrierEnable(pInterfaceDescriptor, kernel.getKernelInfo().patchInfo.executionEnvironment->HasBarriers,
                         kernel.getDevice().getHardwareInfo());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(pInterfaceDescriptor, preemptionMode);
    HardwareCommandsHelper<GfxFamily>::adjustInterfaceDescriptorData(pInterfaceDescriptor, kernel.getDevice().getHardwareInfo());

    pInterfaceDescriptor->setBindingTableEntryCount(bindingTablePrefetchSize);

    return (size_t)offsetInterfaceDescriptor;
}

// Returned binding table pointer is relative to given heap (which is assumed to be the Surface state base addess)
// as required by the INTERFACE_DESCRIPTOR_DATA.
template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, size_t bindingTableCount,
                                                                           const void *srcKernelSsh, size_t srcKernelSshSize,
                                                                           size_t numberOfBindingTableStates, size_t offsetOfBindingTable) {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    if (bindingTableCount == 0) {
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
    BINDING_TABLE_STATE bti = GfxFamily::cmdInitBindingTableState;
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
size_t HardwareCommandsHelper<GfxFamily>::sendIndirectState(
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
    bool localIdsGenerationByRuntime) {

    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    DEBUG_BREAK_IF(simd != 1 && simd != 8 && simd != 16 && simd != 32);
    auto inlineDataProgrammingRequired = HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(kernel);

    // Copy the kernel over to the ISH
    const auto &kernelInfo = kernel.getKernelInfo();
    const auto &patchInfo = kernelInfo.patchInfo;

    auto dstBindingTablePointer = pushBindingTableAndSurfaceStates(ssh, (kernelInfo.patchInfo.bindingTableState != nullptr) ? kernelInfo.patchInfo.bindingTableState->Count : 0,
                                                                   kernel.getSurfaceStateHeap(), kernel.getSurfaceStateHeapSize(),
                                                                   kernel.getNumberOfBindingTableStates(), kernel.getBindingTableOffset());

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

        auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(samplerState);
        for (uint32_t i = 0; i < samplerCount; i++) {
            pSmplr->setIndirectStatePointer((uint32_t)borderColorOffset);
            pSmplr++;
        }
    }

    auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
    DEBUG_BREAK_IF(nullptr == threadPayload);

    auto localWorkItems = localWorkSize[0] * localWorkSize[1] * localWorkSize[2];
    auto threadsPerThreadGroup = static_cast<uint32_t>(getThreadsPerWG(simd, localWorkItems));
    auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);

    uint32_t sizeCrossThreadData = kernel.getCrossThreadDataSize();

    size_t offsetCrossThreadData = HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
        ioh, kernel, inlineDataProgrammingRequired,
        walkerCmd, sizeCrossThreadData);

    size_t sizePerThreadDataTotal = 0;
    size_t sizePerThreadData = 0;

    HardwareCommandsHelper<GfxFamily>::programPerThreadData(
        sizePerThreadData,
        localIdsGenerationByRuntime,
        ioh,
        simd,
        numChannels,
        localWorkSize,
        kernel,
        sizePerThreadDataTotal,
        localWorkItems);

    uint64_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable + interfaceDescriptorIndex * sizeof(INTERFACE_DESCRIPTOR_DATA);
    DEBUG_BREAK_IF(patchInfo.executionEnvironment == nullptr);

    auto bindingTablePrefetchSize = std::min(31u, static_cast<uint32_t>(kernel.getNumberOfBindingTableStates()));
    if (resetBindingTablePrefetch(kernel)) {
        bindingTablePrefetchSize = 0;
    }

    HardwareCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
        dsh,
        offsetInterfaceDescriptor,
        kernelStartOffset,
        sizeCrossThreadData,
        sizePerThreadData,
        dstBindingTablePointer,
        samplerStateOffset,
        samplerCount,
        threadsPerThreadGroup,
        kernel,
        bindingTablePrefetchSize,
        preemptionMode,
        inlineInterfaceDescriptor);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        PatchInfoData patchInfoData(kernelStartOffset, 0, PatchInfoAllocationType::InstructionHeap, dsh.getGraphicsAllocation()->getGpuAddress(), offsetInterfaceDescriptor, PatchInfoAllocationType::DynamicStateHeap);
        kernel.getPatchInfoDataList().push_back(patchInfoData);
    }

    // Program media state flush to set interface descriptor offset
    sendMediaStateFlush(
        commandStream,
        interfaceDescriptorIndex);

    DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
    walkerCmd->setIndirectDataStartAddress(static_cast<uint32_t>(offsetCrossThreadData));
    setInterfaceDescriptorOffset(walkerCmd, interfaceDescriptorIndex);

    auto indirectDataLength = alignUp(static_cast<uint32_t>(sizeCrossThreadData + sizePerThreadDataTotal),
                                      WALKER_TYPE<GfxFamily>::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    walkerCmd->setIndirectDataLength(indirectDataLength);

    return offsetCrossThreadData;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::updatePerThreadDataTotal(
    size_t &sizePerThreadData,
    uint32_t &simd,
    uint32_t &numChannels,
    size_t &sizePerThreadDataTotal,
    size_t &localWorkItems) {
    uint32_t grfSize = sizeof(typename GfxFamily::GRF);
    sizePerThreadData = getPerThreadSizeLocalIDs(simd, grfSize, numChannels);

    uint32_t localIdSizePerThread = PerThreadDataHelper::getLocalIdSizePerThread(simd, grfSize, numChannels);
    localIdSizePerThread = std::max(localIdSizePerThread, grfSize);

    sizePerThreadDataTotal = getThreadsPerWG(simd, localWorkItems) * localIdSizePerThread;
    DEBUG_BREAK_IF(sizePerThreadDataTotal == 0); // Hardware requires at least 1 GRF of perThreadData for each thread in thread group
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programMiSemaphoreWait(LinearStream &commandStream,
                                                               uint64_t compareAddress,
                                                               uint32_t compareData,
                                                               COMPARE_OPERATION compareMode) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    auto miSemaphoreCmd = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    *miSemaphoreCmd = GfxFamily::cmdInitMiSemaphoreWait;
    miSemaphoreCmd->setCompareOperation(compareMode);
    miSemaphoreCmd->setSemaphoreDataDword(compareData);
    miSemaphoreCmd->setSemaphoreGraphicsAddress(compareAddress);
    miSemaphoreCmd->setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

template <typename GfxFamily>
typename GfxFamily::MI_ATOMIC *HardwareCommandsHelper<GfxFamily>::programMiAtomic(LinearStream &commandStream, uint64_t writeAddress,
                                                                                  typename MI_ATOMIC::ATOMIC_OPCODES opcode,
                                                                                  typename MI_ATOMIC::DATA_SIZE dataSize) {
    auto miAtomic = commandStream.getSpaceForCmd<MI_ATOMIC>();
    *miAtomic = GfxFamily::cmdInitAtomic;
    HardwareCommandsHelper<GfxFamily>::programMiAtomic(*miAtomic, writeAddress, opcode, dataSize);
    return miAtomic;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programMiAtomic(MI_ATOMIC &atomic, uint64_t writeAddress,
                                                        typename MI_ATOMIC::ATOMIC_OPCODES opcode,
                                                        typename MI_ATOMIC::DATA_SIZE dataSize) {
    atomic.setAtomicOpcode(opcode);
    atomic.setDataSize(dataSize);
    atomic.setMemoryAddress(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL));
    atomic.setMemoryAddressHigh(static_cast<uint32_t>(writeAddress >> 32));
}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::doBindingTablePrefetch() {
    return true;
}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(const Kernel &kernel) {
    auto checkKernelForInlineData = true;
    if (DebugManager.flags.EnablePassInlineData.get() != -1) {
        checkKernelForInlineData = !!DebugManager.flags.EnablePassInlineData.get();
    }
    if (checkKernelForInlineData) {
        return kernel.getKernelInfo().patchInfo.threadPayload->PassInlineData;
    }
    return false;
}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(const Kernel &kernel) {
    return (kernel.getKernelInfo().patchInfo.threadPayload->LocalIDXPresent ||
            kernel.getKernelInfo().patchInfo.threadPayload->LocalIDYPresent ||
            kernel.getKernelInfo().patchInfo.threadPayload->LocalIDZPresent);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programMiFlushDw(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData) {
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    auto miFlushDwCmd = commandStream.getSpaceForCmd<MI_FLUSH_DW>();
    *miFlushDwCmd = GfxFamily::cmdInitMiFlushDw;
    miFlushDwCmd->setPostSyncOperation(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    miFlushDwCmd->setDestinationAddress(immediateDataGpuAddress);
    miFlushDwCmd->setImmediateData(immediateData);
    appendMiFlushDw(miFlushDwCmd);
}
} // namespace NEO
