/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"

#include "hardware_commands_helper.h"

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
                                                             const size_t localWorkSizes[3], const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto localWorkSize = Math::computeTotalElementsCount(localWorkSizes);
    const auto &kernelDescriptor = kernel.getDescriptor();
    const auto &hwInfo = kernel.getHardwareInfo();

    auto numChannels = kernelDescriptor.kernelAttributes.numLocalIdChannels;
    auto grfCount = kernelDescriptor.kernelAttributes.numGrfRequired;
    uint32_t grfSize = hwInfo.capabilityTable.grfSize;
    auto simdSize = kernelDescriptor.kernelAttributes.simdSize;
    uint32_t requiredWalkOrder = 0u;
    auto isHwLocalIdGeneration = !NEO::EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
        numChannels,
        localWorkSizes,
        std::array<uint8_t, 3>{
            {kernelDescriptor.kernelAttributes.workgroupWalkOrder[0],
             kernelDescriptor.kernelAttributes.workgroupWalkOrder[1],
             kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]}},
        kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder,
        requiredWalkOrder,
        simdSize);
    auto size = kernel.getCrossThreadDataSize() +
                HardwareCommandsHelper::getPerThreadDataSizeTotal(simdSize, grfSize, grfCount, numChannels, localWorkSize, rootDeviceEnvironment);

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        size += ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, isHwLocalIdGeneration, rootDeviceEnvironment);
    }
    return alignUp(size, NEO::EncodeDispatchKernel<GfxFamily>::getDefaultIOHAlignment());
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredSSH(const Kernel &kernel) {
    auto sizeSSH = kernel.getSurfaceStateHeapSize();
    sizeSSH += sizeSSH ? GfxFamily::cacheLineSize : 0;
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
                                                                                         dispatchInfo.getLocalWorkgroupSize().values,
                                                                                         dispatchInfo.getClDevice().getRootDeviceEnvironment()); });
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(
    const MultiDispatchInfo &multiDispatchInfo) {
    return getSizeRequired(multiDispatchInfo, [](const DispatchInfo &dispatchInfo) { return getSizeRequiredSSH(*dispatchInfo.getKernel()); });
}

template <typename GfxFamily>
template <typename WalkerType, typename InterfaceDescriptorType>
size_t HardwareCommandsHelper<GfxFamily>::sendInterfaceDescriptorData(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    uint64_t kernelStartOffset,
    size_t sizeCrossThreadData,
    size_t sizePerThreadData,
    [[maybe_unused]] size_t bindingTablePointer,
    [[maybe_unused]] size_t offsetSamplerState,
    [[maybe_unused]] uint32_t numSamplers,
    const uint32_t threadGroupCount,
    uint32_t threadsPerThreadGroup,
    const Kernel &kernel,
    [[maybe_unused]] uint32_t bindingTablePrefetchSize,
    PreemptionMode preemptionMode,
    const Device &device,
    WalkerType *walkerCmd,
    InterfaceDescriptorType *inlineInterfaceDescriptor) {

    constexpr bool heaplessModeEnabled = GfxFamily::template isHeaplessMode<WalkerType>();

    // Allocate some memory for the interface descriptor
    InterfaceDescriptorType *pInterfaceDescriptor = nullptr;
    auto interfaceDescriptor = GfxFamily::template getInitInterfaceDescriptor<InterfaceDescriptorType>();

    if constexpr (heaplessModeEnabled) {
        pInterfaceDescriptor = inlineInterfaceDescriptor;
        interfaceDescriptor.setKernelStartPointer(kernelStartOffset);
    } else {
        pInterfaceDescriptor = HardwareCommandsHelper::getInterfaceDescriptor(indirectHeap, offsetInterfaceDescriptor, inlineInterfaceDescriptor);
        interfaceDescriptor.setKernelStartPointer(static_cast<uint32_t>(kernelStartOffset));
    }

    //  # of threads in thread group should be based on LWS.
    interfaceDescriptor.setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    auto slmTotalSize = kernel.getSlmTotalSize();
    const auto &kernelDescriptor = kernel.getKernelInfo().kernelDescriptor;

    EncodeDispatchKernel<GfxFamily>::setGrfInfo(&interfaceDescriptor, kernelDescriptor.kernelAttributes.numGrfRequired,
                                                sizeCrossThreadData, sizePerThreadData, device.getRootDeviceEnvironment());

    EncodeDispatchKernel<GfxFamily>::setupPreferredSlmSize(&interfaceDescriptor, device.getRootDeviceEnvironment(),
                                                           threadsPerThreadGroup, slmTotalSize, SlmPolicy::slmPolicyNone);

    if constexpr (heaplessModeEnabled == false) {
        interfaceDescriptor.setBindingTablePointer(static_cast<uint32_t>(bindingTablePointer));

        if constexpr (GfxFamily::supportsSampler) {
            if (device.getDeviceInfo().imageSupport) {
                interfaceDescriptor.setSamplerStatePointer(static_cast<uint32_t>(offsetSamplerState));
            }
        }
        EncodeDispatchKernel<GfxFamily>::adjustBindingTablePrefetch(interfaceDescriptor, numSamplers, bindingTablePrefetchSize);
    }

    const auto &hardwareInfo = device.getHardwareInfo();
    auto &gfxCoreHelper = device.getGfxCoreHelper();
    auto releaseHelper = device.getReleaseHelper();
    auto programmableIDSLMSize = EncodeDispatchKernel<GfxFamily>::computeSlmValues(hardwareInfo, slmTotalSize, releaseHelper, heaplessModeEnabled);

    if (debugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        programmableIDSLMSize = static_cast<uint32_t>(debugManager.flags.OverrideSlmAllocationSize.get());
    }

    interfaceDescriptor.setSharedLocalMemorySize(programmableIDSLMSize);
    EncodeDispatchKernel<GfxFamily>::programBarrierEnable(interfaceDescriptor,
                                                          kernelDescriptor,
                                                          hardwareInfo);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<GfxFamily>(&interfaceDescriptor, preemptionMode);

    auto defaultPipelinedThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    if (NEO::debugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        defaultPipelinedThreadArbitrationPolicy = NEO::debugManager.flags.OverrideThreadArbitrationPolicy.get();
    }
    EncodeDispatchKernel<GfxFamily>::encodeEuSchedulingPolicy(&interfaceDescriptor, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
    const uint32_t threadGroupDimensions[] = {walkerCmd->getThreadGroupIdXDimension(), walkerCmd->getThreadGroupIdYDimension(), walkerCmd->getThreadGroupIdXDimension()};
    EncodeDispatchKernel<GfxFamily>::encodeThreadGroupDispatch(interfaceDescriptor, device, hardwareInfo, threadGroupDimensions, threadGroupCount, kernelDescriptor.kernelMetadata.requiredThreadGroupDispatchSize,
                                                               kernelDescriptor.kernelAttributes.numGrfRequired, threadsPerThreadGroup, *walkerCmd);

    *pInterfaceDescriptor = interfaceDescriptor;
    return static_cast<size_t>(offsetInterfaceDescriptor);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programPerThreadData(
    bool localIdsGenerationByRuntime,
    size_t &sizePerThreadData,
    size_t &sizePerThreadDataTotal,
    LinearStream &ioh,
    const Kernel &kernel,
    const size_t localWorkSize[3]) {
    if (localIdsGenerationByRuntime) {
        Vec3<uint16_t> group = {static_cast<uint16_t>(localWorkSize[0]),
                                static_cast<uint16_t>(localWorkSize[1]),
                                static_cast<uint16_t>(localWorkSize[2])};
        sizePerThreadData = kernel.getLocalIdsSizePerThread();
        sizePerThreadDataTotal = kernel.getLocalIdsSizeForGroup(group);
        auto dest = ioh.getSpace(sizePerThreadDataTotal);
        kernel.setLocalIdsForGroup(group, dest);
    }
}

template <typename GfxFamily>
template <typename WalkerType, typename InterfaceDescriptorType>
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
    WalkerType *walkerCmd,
    InterfaceDescriptorType *inlineInterfaceDescriptor,
    bool localIdsGenerationByRuntime,
    uint64_t scratchAddress,
    const Device &device) {

    DEBUG_BREAK_IF(simd != 1 && simd != 8 && simd != 16 && simd != 32);
    // Copy the kernel over to the ISH
    constexpr bool heaplessModeEnabled = GfxFamily::template isHeaplessMode<WalkerType>();
    constexpr bool bindfulAllowed = !heaplessModeEnabled;

    size_t dstBindingTablePointer = 0;
    uint32_t samplerCount = 0;
    uint32_t samplerStateOffset = 0;
    uint32_t bindingTablePrefetchSize = 0;

    ssh.align(GfxFamily::cacheLineSize);
    dstBindingTablePointer = HardwareCommandsHelper<GfxFamily>::checkForAdditionalBTAndSetBTPointer(ssh, kernel);

    const auto &kernelInfo = kernel.getKernelInfo();
    // Copy our sampler state if it exists
    const auto &samplerTable = kernelInfo.kernelDescriptor.payloadMappings.samplerTable;
    if (isValidOffset(samplerTable.tableOffset) && isValidOffset(samplerTable.borderColor)) {
        samplerCount = samplerTable.numSamplers;
        samplerStateOffset = EncodeStates<GfxFamily>::copySamplerState(&dsh, samplerTable.tableOffset,
                                                                       samplerCount, samplerTable.borderColor,
                                                                       kernel.getDynamicStateHeap(), device.getBindlessHeapsHelper(),
                                                                       device.getRootDeviceEnvironment());
        if constexpr (heaplessModeEnabled) {
            uint64_t bindlessSamplerStateAddress = samplerStateOffset;
            bindlessSamplerStateAddress += dsh.getGraphicsAllocation()->getGpuAddress();
            kernel.patchBindlessSamplerStatesInCrossThreadData(bindlessSamplerStateAddress);
        }
    }

    if constexpr (bindfulAllowed) {
        if (EncodeSurfaceState<GfxFamily>::doBindingTablePrefetch()) {
            bindingTablePrefetchSize = std::min(31u, static_cast<uint32_t>(kernel.getNumberOfBindingTableStates()));
        }
    }

    const bool isBindlessKernel = NEO::KernelDescriptor::isBindlessAddressingKernel(kernel.getKernelInfo().kernelDescriptor);
    if (isBindlessKernel) {
        uint64_t bindlessSurfaceStatesBaseAddress = ptrDiff(ssh.getSpace(0), ssh.getCpuBase());
        if constexpr (heaplessModeEnabled) {
            bindlessSurfaceStatesBaseAddress += ssh.getGraphicsAllocation()->getGpuAddress();
        }

        auto sshHeapSize = kernel.getSurfaceStateHeapSize();
        // Allocate space for new ssh data
        auto dstSurfaceState = ssh.getSpace(sshHeapSize);
        memcpy_s(dstSurfaceState, sshHeapSize, kernel.getSurfaceStateHeap(), sshHeapSize);

        kernel.patchBindlessSurfaceStatesInCrossThreadData<heaplessModeEnabled>(bindlessSurfaceStatesBaseAddress);
    }

    auto &gfxCoreHelper = device.getGfxCoreHelper();
    auto grfCount = kernel.getDescriptor().kernelAttributes.numGrfRequired;
    auto localWorkItems = localWorkSize[0] * localWorkSize[1] * localWorkSize[2];
    auto threadsPerThreadGroup = gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, static_cast<uint32_t>(localWorkItems), grfCount, device.getRootDeviceEnvironment());

    uint32_t sizeCrossThreadData = kernel.getCrossThreadDataSize();

    auto inlineDataProgrammingRequired = EncodeDispatchKernel<GfxFamily>::inlineDataProgrammingRequired(kernel.getKernelInfo().kernelDescriptor);
    size_t offsetCrossThreadData = HardwareCommandsHelper<GfxFamily>::sendCrossThreadData<WalkerType>(
        ioh, kernel, inlineDataProgrammingRequired,
        walkerCmd, sizeCrossThreadData, scratchAddress, device.getRootDeviceEnvironment());

    size_t sizePerThreadDataTotal = 0;
    size_t sizePerThreadData = 0;

    HardwareCommandsHelper<GfxFamily>::programPerThreadData(
        localIdsGenerationByRuntime,
        sizePerThreadData,
        sizePerThreadDataTotal,
        ioh,
        kernel,
        localWorkSize);

    uint64_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable + interfaceDescriptorIndex * GfxFamily::template getInterfaceDescriptorSize<WalkerType>();

    HardwareCommandsHelper<GfxFamily>::sendInterfaceDescriptorData<WalkerType, InterfaceDescriptorType>(
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
        device,
        walkerCmd,
        inlineInterfaceDescriptor);

    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        PatchInfoData patchInfoData(kernelStartOffset, 0, PatchInfoAllocationType::instructionHeap, dsh.getGraphicsAllocation()->getGpuAddress(), offsetInterfaceDescriptor, PatchInfoAllocationType::dynamicStateHeap);
        kernel.getPatchInfoDataList().push_back(patchInfoData);
    }

    if constexpr (heaplessModeEnabled == false) {
        // Program media state flush to set interface descriptor offset
        sendMediaStateFlush(
            commandStream,
            interfaceDescriptorIndex);

        DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
        walkerCmd->setIndirectDataStartAddress(static_cast<uint32_t>(offsetCrossThreadData));
        setInterfaceDescriptorOffset(walkerCmd, interfaceDescriptorIndex);
        auto indirectDataLength = alignUp(static_cast<uint32_t>(sizeCrossThreadData + sizePerThreadDataTotal),
                                          WalkerType::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        walkerCmd->setIndirectDataLength(indirectDataLength);
    }
    ioh.align(NEO::EncodeDispatchKernel<GfxFamily>::getDefaultIOHAlignment());

    return offsetCrossThreadData;
}

template <typename GfxFamily>
template <typename WalkerType>
void HardwareCommandsHelper<GfxFamily>::programInlineData(Kernel &kernel, WalkerType *walkerCmd, uint64_t indirectDataAddress, uint64_t scratchAddress) {}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(const Kernel &kernel) {
    return kernel.getKernelInfo().kernelDescriptor.kernelAttributes.numLocalIdChannels > 0;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::checkForAdditionalBTAndSetBTPointer(IndirectHeap &ssh, const Kernel &kernel) {
    size_t dstBindingTablePointer{0u};
    const auto &kernelInfo = kernel.getKernelInfo();
    if (false == isGTPinInitialized && 0u == kernelInfo.kernelDescriptor.payloadMappings.bindingTable.numEntries) {
        dstBindingTablePointer = 0u;
    } else {
        dstBindingTablePointer = EncodeSurfaceState<GfxFamily>::pushBindingTableAndSurfaceStates(ssh,
                                                                                                 kernel.getSurfaceStateHeap(), kernel.getSurfaceStateHeapSize(),
                                                                                                 kernel.getNumberOfBindingTableStates(), kernel.getBindingTableOffset());
    }
    return dstBindingTablePointer;
}

} // namespace NEO
