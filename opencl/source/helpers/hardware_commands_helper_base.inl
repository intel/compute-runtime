/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredDSH(const Kernel &kernel) {
    constexpr auto samplerStateSize = sizeof(typename GfxFamily::SAMPLER_STATE);
    constexpr auto maxIndirectSamplerStateSize = alignUp(sizeof(typename GfxFamily::SAMPLER_BORDER_COLOR_STATE), MemoryConstants::cacheLineSize);
    const auto numSamplers = kernel.getKernelInfo().kernelDescriptor.payloadMappings.samplerTable.numSamplers;

    if (numSamplers == 0U) {
        return alignUp(additionalSizeRequiredDsh(), MemoryConstants::cacheLineSize);
    }

    auto calculatedTotalSize = alignUp(maxIndirectSamplerStateSize + numSamplers * samplerStateSize + additionalSizeRequiredDsh(),
                                       MemoryConstants::cacheLineSize);
    DEBUG_BREAK_IF(calculatedTotalSize > kernel.getDynamicStateHeapSize());
    return calculatedTotalSize;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredIOH(const Kernel &kernel,
                                                             size_t localWorkSize) {
    typedef typename GfxFamily::WALKER_TYPE WALKER_TYPE;
    const auto &kernelDescriptor = kernel.getDescriptor();
    const auto &hwInfo = kernel.getHardwareInfo();

    auto numChannels = kernelDescriptor.kernelAttributes.numLocalIdChannels;
    uint32_t grfSize = hwInfo.capabilityTable.grfSize;
    auto simdSize = kernelDescriptor.kernelAttributes.simdSize;
    auto size = kernel.getCrossThreadDataSize() +
                getPerThreadDataSizeTotal(simdSize, grfSize, numChannels, localWorkSize);

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        size += ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, hwInfo);
    }
    return alignUp(size, WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredSSH(const Kernel &kernel) {
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
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredIOH(
                                                                                         *dispatchInfo.getKernel(),
                                                                                         Math::computeTotalElementsCount(dispatchInfo.getLocalWorkgroupSize())); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredSSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    uint64_t kernelStartOffset,
    size_t sizeCrossThreadData,
    size_t sizePerThreadData,
    size_t bindingTablePointer,
    [[maybe_unused]] size_t offsetSamplerState,
    uint32_t numSamplers,
    const uint32_t threadGroupCount,
    uint32_t threadsPerThreadGroup,
    const Kernel &kernel,
    uint32_t bindingTablePrefetchSize,
    PreemptionMode preemptionMode,
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor,
    const Device &device) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    using SHARED_LOCAL_MEMORY_SIZE = typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    const auto &hardwareInfo = device.getHardwareInfo();
    const auto &kernelDescriptor = kernel.getKernelInfo().kernelDescriptor;

    // Allocate some memory for the interface descriptor
    auto pInterfaceDescriptor = getInterfaceDescriptor(indirectHeap, offsetInterfaceDescriptor, inlineInterfaceDescriptor);
    auto interfaceDescriptor = GfxFamily::cmdInitInterfaceDescriptorData;

    // Program the kernel start pointer
    interfaceDescriptor.setKernelStartPointer(static_cast<uint32_t>(kernelStartOffset & std::numeric_limits<uint32_t>::max()));

    // # of threads in thread group should be based on LWS.
    interfaceDescriptor.setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    interfaceDescriptor.setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);

    auto slmTotalSize = kernel.getSlmTotalSize();

    EncodeDispatchKernel<GfxFamily>::setGrfInfo(&interfaceDescriptor, kernelDescriptor.kernelAttributes.numGrfRequired,
                                                sizeCrossThreadData, sizePerThreadData, hardwareInfo);
    auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hwInfoConfig.updateIddCommand(&interfaceDescriptor, kernelDescriptor.kernelAttributes.numGrfRequired,
                                  kernelDescriptor.kernelAttributes.threadArbitrationPolicy);

    EncodeDispatchKernel<GfxFamily>::appendAdditionalIDDFields(&interfaceDescriptor, hardwareInfo, threadsPerThreadGroup,
                                                               slmTotalSize, SlmPolicy::SlmPolicyNone);

    interfaceDescriptor.setBindingTablePointer(static_cast<uint32_t>(bindingTablePointer));

    if constexpr (GfxFamily::supportsSampler) {
        if (device.getDeviceInfo().imageSupport) {
            interfaceDescriptor.setSamplerStatePointer(static_cast<uint32_t>(offsetSamplerState));
        }
    }

    EncodeDispatchKernel<GfxFamily>::adjustBindingTablePrefetch(interfaceDescriptor, numSamplers, bindingTablePrefetchSize);

    auto programmableIDSLMSize =
        static_cast<SHARED_LOCAL_MEMORY_SIZE>(HwHelperHw<GfxFamily>::get().computeSlmValues(hardwareInfo, slmTotalSize));

    if (DebugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        programmableIDSLMSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(DebugManager.flags.OverrideSlmAllocationSize.get());
    }

    interfaceDescriptor.setSharedLocalMemorySize(programmableIDSLMSize);
    EncodeDispatchKernel<GfxFamily>::programBarrierEnable(interfaceDescriptor,
                                                          kernelDescriptor.kernelAttributes.barrierCount,
                                                          hardwareInfo);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(&interfaceDescriptor, preemptionMode);

    EncodeDispatchKernel<GfxFamily>::adjustInterfaceDescriptorData(interfaceDescriptor, hardwareInfo, threadGroupCount, kernelDescriptor.kernelAttributes.numGrfRequired);

    *pInterfaceDescriptor = interfaceDescriptor;
    return (size_t)offsetInterfaceDescriptor;
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
    const uint32_t threadGroupCount,
    const uint64_t offsetInterfaceDescriptorTable,
    uint32_t &interfaceDescriptorIndex,
    PreemptionMode preemptionMode,
    WALKER_TYPE *walkerCmd,
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor,
    bool localIdsGenerationByRuntime,
    const Device &device) {

    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    auto rootDeviceIndex = device.getRootDeviceIndex();

    DEBUG_BREAK_IF(simd != 1 && simd != 8 && simd != 16 && simd != 32);
    auto inlineDataProgrammingRequired = HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(kernel);

    // Copy the kernel over to the ISH
    const auto &kernelInfo = kernel.getKernelInfo();

    ssh.align(BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

    auto dstBindingTablePointer = EncodeSurfaceState<GfxFamily>::pushBindingTableAndSurfaceStates(ssh, kernelInfo.kernelDescriptor.payloadMappings.bindingTable.numEntries,
                                                                                                  kernel.getSurfaceStateHeap(), kernel.getSurfaceStateHeapSize(),
                                                                                                  kernel.getNumberOfBindingTableStates(), kernel.getBindingTableOffset());

    // Copy our sampler state if it exists
    const auto &samplerTable = kernelInfo.kernelDescriptor.payloadMappings.samplerTable;
    uint32_t samplerCount = 0;
    uint32_t samplerStateOffset = 0;
    if (isValidOffset(samplerTable.tableOffset) && isValidOffset(samplerTable.borderColor)) {
        samplerCount = samplerTable.numSamplers;
        samplerStateOffset = EncodeStates<GfxFamily>::copySamplerState(&dsh, samplerTable.tableOffset,
                                                                       samplerCount, samplerTable.borderColor,
                                                                       kernel.getDynamicStateHeap(), device.getBindlessHeapsHelper(),
                                                                       device.getHardwareInfo());
    }

    auto localWorkItems = localWorkSize[0] * localWorkSize[1] * localWorkSize[2];
    auto threadsPerThreadGroup = static_cast<uint32_t>(getThreadsPerWG(simd, localWorkItems));
    auto numChannels = static_cast<uint32_t>(kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels);

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
        localWorkItems,
        rootDeviceIndex);

    uint64_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable + interfaceDescriptorIndex * sizeof(INTERFACE_DESCRIPTOR_DATA);

    auto bindingTablePrefetchSize = 0;
    if (EncodeSurfaceState<GfxFamily>::doBindingTablePrefetch()) {
        bindingTablePrefetchSize = std::min(31u, static_cast<uint32_t>(kernel.getNumberOfBindingTableStates()));
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
        threadGroupCount,
        threadsPerThreadGroup,
        kernel,
        bindingTablePrefetchSize,
        preemptionMode,
        inlineInterfaceDescriptor,
        device);

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
                                      WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
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
bool HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(const Kernel &kernel) {
    auto checkKernelForInlineData = true;
    if (DebugManager.flags.EnablePassInlineData.get() != -1) {
        checkKernelForInlineData = !!DebugManager.flags.EnablePassInlineData.get();
    }
    if (checkKernelForInlineData) {
        return kernel.getKernelInfo().kernelDescriptor.kernelAttributes.flags.passInlineData;
    }
    return false;
}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(const Kernel &kernel) {
    return kernel.getKernelInfo().kernelDescriptor.kernelAttributes.numLocalIdChannels > 0;
}

} // namespace NEO
