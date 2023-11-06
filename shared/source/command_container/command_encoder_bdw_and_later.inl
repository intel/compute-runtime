/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args.h"

#include <algorithm>

namespace NEO {

template <typename Family>
void EncodeDispatchKernel<Family>::setGrfInfo(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t numGrf,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto grfSize = sizeof(typename Family::GRF);
    DEBUG_BREAK_IF((sizeCrossThreadData % grfSize) != 0);
    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / grfSize);
    DEBUG_BREAK_IF(numGrfCrossThreadData == 0);
    pInterfaceDescriptor->setCrossThreadConstantDataReadLength(numGrfCrossThreadData);

    DEBUG_BREAK_IF((sizePerThreadData % grfSize) != 0);
    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / grfSize);

    // at least 1 GRF of perThreadData for each thread in a thread group when sizeCrossThreadData != 0
    numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);
    pInterfaceDescriptor->setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);
}

template <typename Family>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container, EncodeDispatchKernelArgs &args) {

    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = args.dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = args.dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = args.dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto pImplicitArgs = args.dispatchInterface->getImplicitArgs();

    auto &hwInfo = args.device->getHardwareInfo();
    auto &gfxCoreHelper = args.device->getGfxCoreHelper();
    auto &rootDeviceEnvironment = args.device->getRootDeviceEnvironment();

    LinearStream *listCmdBufferStream = container.getCommandStream();

    auto threadDims = static_cast<const uint32_t *>(args.threadGroupDimensions);
    const Vec3<size_t> threadStartVec{0, 0, 0};
    Vec3<size_t> threadDimsVec{0, 0, 0};
    if (!args.isIndirect) {
        threadDimsVec = {threadDims[0], threadDims[1], threadDims[2]};
    }

    WALKER_TYPE cmd = Family::cmdInitGpgpuWalker;
    auto idd = Family::cmdInitInterfaceDescriptorData;
    {
        auto alloc = args.dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch() + args.dispatchInterface->getIsaOffsetInParentAllocation();
        idd.setKernelStartPointer(offset);
        idd.setKernelStartPointerHigh(0u);
    }

    bool debugEnabled = args.device->getL0Debugger() != nullptr;
    if (args.dispatchInterface->getKernelDescriptor().kernelAttributes.flags.usesAssert && debugEnabled) {
        idd.setSoftwareExceptionEnable(1);
    }

    auto numThreadsPerThreadGroup = args.dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(numThreadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor.kernelAttributes.barrierCount,
                                                       hwInfo);
    auto slmSize = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        gfxCoreHelper.computeSlmValues(hwInfo, args.dispatchInterface->getSlmTotalSize()));
    idd.setSharedLocalMemorySize(slmSize);

    uint32_t bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    uint32_t bindingTablePointer = 0u;
    bool isBindlessKernel = NEO::KernelDescriptor::isBindlessAddressingKernel(kernelDescriptor);

    if (!isBindlessKernel) {
        container.prepareBindfulSsh();
        if (bindingTableStateCount > 0u) {
            auto ssh = args.surfaceStateHeap;
            if (ssh == nullptr) {
                ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, args.dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            }
            bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                *ssh,
                args.dispatchInterface->getSurfaceStateHeapData(),
                args.dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                kernelDescriptor.payloadMappings.bindingTable.tableOffset));
        }
    } else {
        bool globalBindlessSsh = args.device->getBindlessHeapsHelper() != nullptr;
        if (args.dispatchInterface->getSurfaceStateHeapDataSize() > 0u) {
            auto ssh = args.surfaceStateHeap;
            if (ssh == nullptr) {
                container.prepareBindfulSsh();
                ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, args.dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            }
            uint64_t bindlessSshBaseOffset = ptrDiff(ssh->getSpace(0), ssh->getCpuBase());
            if (globalBindlessSsh) {
                bindlessSshBaseOffset += ptrDiff(ssh->getGraphicsAllocation()->getGpuAddress(), ssh->getGraphicsAllocation()->getGpuBaseAddress());
            }
            // Allocate space for new ssh data
            auto dstSurfaceState = ssh->getSpace(args.dispatchInterface->getSurfaceStateHeapDataSize());
            memcpy_s(dstSurfaceState, args.dispatchInterface->getSurfaceStateHeapDataSize(), args.dispatchInterface->getSurfaceStateHeapData(), args.dispatchInterface->getSurfaceStateHeapDataSize());
            args.dispatchInterface->patchBindlessOffsetsInCrossThreadData(bindlessSshBaseOffset);
        }
    }
    idd.setBindingTablePointer(bindingTablePointer);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, args.preemptionMode);

    uint32_t samplerStateOffset = 0;
    uint32_t samplerCount = 0;

    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
        auto dsHeap = args.dynamicStateHeap;
        if (dsHeap == nullptr) {
            dsHeap = container.getIndirectHeap(HeapType::DYNAMIC_STATE);
            auto dshSizeRequired = NEO::EncodeDispatchKernel<Family>::getSizeRequiredDsh(kernelDescriptor, container.getNumIddPerBlock());
            if (dsHeap->getAvailableSpace() <= dshSizeRequired) {
                dsHeap = container.getHeapWithRequiredSizeAndAlignment(HeapType::DYNAMIC_STATE, dsHeap->getMaxAvailableSpace(), NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
            }
        }
        UNRECOVERABLE_IF(!dsHeap);

        samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
        samplerStateOffset = EncodeStates<Family>::copySamplerState(dsHeap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                                                                    kernelDescriptor.payloadMappings.samplerTable.numSamplers,
                                                                    kernelDescriptor.payloadMappings.samplerTable.borderColor,
                                                                    args.dispatchInterface->getDynamicStateHeapData(),
                                                                    args.device->getBindlessHeapsHelper(), args.device->getRootDeviceEnvironment());
    }

    idd.setSamplerStatePointer(samplerStateOffset);
    if (!isBindlessKernel) {
        EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, samplerCount, bindingTableStateCount);
    }

    EncodeDispatchKernel<Family>::setGrfInfo(&idd, kernelDescriptor.kernelAttributes.numGrfRequired, sizeCrossThreadData,
                                             sizePerThreadData, rootDeviceEnvironment);

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    bool isHwLocalIdGeneration = false;
    uint32_t sizeForImplicitArgsPatching = NEO::ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, isHwLocalIdGeneration, gfxCoreHelper);
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching;
    uint64_t offsetThreadData = 0u;
    {
        auto heapIndirect = container.getIndirectHeap(HeapType::INDIRECT_OBJECT);
        UNRECOVERABLE_IF(!(heapIndirect));
        heapIndirect->align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        void *ptr = nullptr;
        if (args.isKernelDispatchedFromImmediateCmdList) {
            ptr = container.getHeapWithRequiredSizeAndAlignment(HeapType::INDIRECT_OBJECT, iohRequiredSize, WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE)->getSpace(iohRequiredSize);
        } else {
            ptr = container.getHeapSpaceAllowGrow(HeapType::INDIRECT_OBJECT, iohRequiredSize);
        }
        UNRECOVERABLE_IF(!(ptr));
        offsetThreadData = heapIndirect->getHeapGpuStartOffset() + static_cast<uint64_t>(heapIndirect->getUsed() - sizeThreadData);

        uint64_t implicitArgsGpuVA = 0u;
        if (pImplicitArgs) {
            implicitArgsGpuVA = heapIndirect->getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(heapIndirect->getUsed() - iohRequiredSize);
            auto implicitArgsCrossThreadPtr = ptrOffset(const_cast<uint64_t *>(reinterpret_cast<const uint64_t *>(args.dispatchInterface->getCrossThreadData())), kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
            *implicitArgsCrossThreadPtr = implicitArgsGpuVA;

            ptr = NEO::ImplicitArgsHelper::patchImplicitArgs(ptr, *pImplicitArgs, kernelDescriptor, {}, gfxCoreHelper);
        }

        memcpy_s(ptr, sizeCrossThreadData,
                 args.dispatchInterface->getCrossThreadData(), sizeCrossThreadData);

        if (args.isIndirect) {
            auto crossThreadDataGpuVA = heapIndirect->getGraphicsAllocation()->getGpuAddress() + heapIndirect->getUsed() - sizeThreadData;
            EncodeIndirectParams<Family>::encode(container, crossThreadDataGpuVA, args.dispatchInterface, implicitArgsGpuVA);
        }

        ptr = ptrOffset(ptr, sizeCrossThreadData);
        memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                 args.dispatchInterface->getPerThreadData(), sizePerThreadDataForWholeGroup);
    }

    uint32_t numIDD = 0u;
    void *iddPtr = getInterfaceDescriptor(container, args.dynamicStateHeap, numIDD);

    auto slmSizeNew = args.dispatchInterface->getSlmTotalSize();
    bool dirtyHeaps = container.isAnyHeapDirty();
    bool flush = container.slmSizeRef() != slmSizeNew || dirtyHeaps || args.requiresUncachedMocs;

    if (flush) {
        PipeControlArgs syncArgs;
        syncArgs.dcFlushEnable = args.dcFlushEnable;
        if (dirtyHeaps) {
            syncArgs.hdcPipelineFlush = true;
        }
        MemorySynchronizationCommands<Family>::addSingleBarrier(*container.getCommandStream(), syncArgs);

        if (dirtyHeaps || args.requiresUncachedMocs) {
            STATE_BASE_ADDRESS sba;
            auto gmmHelper = container.getDevice()->getGmmHelper();
            uint32_t statelessMocsIndex =
                args.requiresUncachedMocs ? (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1) : (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
            auto l1CachePolicy = container.l1CachePolicyDataRef()->getL1CacheValue(false);
            auto l1CachePolicyDebuggerActive = container.l1CachePolicyDataRef()->getL1CacheValue(true);
            EncodeStateBaseAddressArgs<Family> encodeStateBaseAddressArgs = {
                &container,                  // container
                sba,                         // sbaCmd
                nullptr,                     // sbaProperties
                statelessMocsIndex,          // statelessMocsIndex
                l1CachePolicy,               // l1CachePolicy
                l1CachePolicyDebuggerActive, // l1CachePolicyDebuggerActive
                false,                       // useGlobalAtomics
                false,                       // multiOsContextCapable
                args.isRcs,                  // isRcs
                container.doubleSbaWaRef()}; // doubleSbaWa
            EncodeStateBaseAddress<Family>::encode(encodeStateBaseAddressArgs);
            container.setDirtyStateForAllHeaps(false);
            args.requiresUncachedMocs = false;
        }

        if (container.slmSizeRef() != slmSizeNew) {
            EncodeL3State<Family>::encode(container, slmSizeNew != 0u);
            container.slmSizeRef() = slmSizeNew;
        }
    }

    if (numIDD == 0 || flush) {
        EncodeMediaInterfaceDescriptorLoad<Family>::encode(container, args.dynamicStateHeap);
    }

    cmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
    cmd.setIndirectDataLength(sizeThreadData);
    cmd.setInterfaceDescriptorOffset(numIDD);

    EncodeDispatchKernel<Family>::encodeThreadData(cmd,
                                                   nullptr,
                                                   threadDims,
                                                   args.dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   args.dispatchInterface->getNumThreadsPerThreadGroup(),
                                                   args.dispatchInterface->getThreadExecutionMask(),
                                                   true,
                                                   false,
                                                   args.isIndirect,
                                                   args.dispatchInterface->getRequiredWorkgroupOrder(),
                                                   rootDeviceEnvironment);

    cmd.setPredicateEnable(args.isPredicate);

    auto threadGroupCount = cmd.getThreadGroupIdXDimension() * cmd.getThreadGroupIdYDimension() * cmd.getThreadGroupIdZDimension();
    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(idd, *args.device, hwInfo, threadGroupCount, kernelDescriptor.kernelAttributes.numGrfRequired, cmd);

    memcpy_s(iddPtr, sizeof(idd), &idd, sizeof(idd));

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::DebugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(args.device->getRootDeviceEnvironment(), false));
        args.additionalCommands->push_back(commandBuffer);

        EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
    }

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *args.device);

    auto buffer = listCmdBufferStream->getSpaceForCmd<WALKER_TYPE>();
    *buffer = cmd;

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *args.device);
    {
        auto mediaStateFlush = listCmdBufferStream->getSpaceForCmd<MEDIA_STATE_FLUSH>();
        *mediaStateFlush = Family::cmdInitMediaStateFlush;
    }

    args.partitionCount = 1;

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::DebugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(args.device->getRootDeviceEnvironment(), false));
        args.additionalCommands->push_back(commandBuffer);

        EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
    }
}

template <typename Family>
void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container, IndirectHeap *childDsh) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    void *heapBase = nullptr;
    if (childDsh != nullptr) {
        heapBase = childDsh->getCpuBase();
    } else {
        heapBase = container.getIndirectHeap(HeapType::DYNAMIC_STATE)->getCpuBase();
    }

    auto mediaStateFlush = container.getCommandStream()->getSpaceForCmd<MEDIA_STATE_FLUSH>();
    *mediaStateFlush = Family::cmdInitMediaStateFlush;

    auto iddOffset = static_cast<uint32_t>(ptrDiff(container.getIddBlock(), heapBase));

    MEDIA_INTERFACE_DESCRIPTOR_LOAD cmd = Family::cmdInitMediaInterfaceDescriptorLoad;
    cmd.setInterfaceDescriptorDataStartAddress(iddOffset);
    cmd.setInterfaceDescriptorTotalLength(sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock());

    auto buffer = container.getCommandStream()->getSpace(sizeof(cmd));
    *(decltype(cmd) *)buffer = cmd;
}

template <typename Family>
inline bool EncodeDispatchKernel<Family>::isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                                              const size_t *lws,
                                                                              std::array<uint8_t, 3> walkOrder,
                                                                              bool requireInputWalkOrder,
                                                                              uint32_t &requiredWalkOrder,
                                                                              uint32_t simd) {
    requiredWalkOrder = 0u;
    return true;
}

template <typename Family>
void EncodeDispatchKernel<Family>::encodeThreadData(WALKER_TYPE &walkerCmd,
                                                    const uint32_t *startWorkGroup,
                                                    const uint32_t *numWorkGroups,
                                                    const uint32_t *workGroupSizes,
                                                    uint32_t simd,
                                                    uint32_t localIdDimensions,
                                                    uint32_t threadsPerThreadGroup,
                                                    uint32_t threadExecutionMask,
                                                    bool localIdsGenerationByRuntime,
                                                    bool inlineDataProgrammingRequired,
                                                    bool isIndirect,
                                                    uint32_t requiredWorkGroupOrder,
                                                    const RootDeviceEnvironment &rootDeviceEnvironment) {

    if (isIndirect) {
        walkerCmd.setIndirectParameterEnable(true);
    } else {
        walkerCmd.setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
        walkerCmd.setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
        walkerCmd.setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));
    }

    if (startWorkGroup) {
        walkerCmd.setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroup[0]));
        walkerCmd.setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroup[1]));
        walkerCmd.setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroup[2]));
    }

    walkerCmd.setSimdSize(getSimdConfig<WALKER_TYPE>(simd));

    auto localWorkSize = static_cast<uint32_t>(workGroupSizes[0] * workGroupSizes[1] * workGroupSizes[2]);
    if (threadsPerThreadGroup == 0) {
        threadsPerThreadGroup = getThreadsPerWG(simd, localWorkSize);
    }
    walkerCmd.setThreadWidthCounterMaximum(threadsPerThreadGroup);

    uint64_t executionMask = threadExecutionMask;
    if (executionMask == 0) {
        auto remainderSimdLanes = localWorkSize & (simd - 1);
        executionMask = maxNBitValue(remainderSimdLanes);
        if (!executionMask)
            executionMask = ~executionMask;
    }

    constexpr uint32_t maxDword = std::numeric_limits<uint32_t>::max();
    walkerCmd.setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd.setBottomExecutionMask(maxDword);
}

template <typename Family>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor,
                                                        uint32_t value,
                                                        const HardwareInfo &hwInfo) {
    interfaceDescriptor.setBarrierEnable(value);
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WALKER_TYPE &walkerCmd, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {}

template <typename Family>
inline bool EncodeDispatchKernel<Family>::isDshNeeded(const DeviceInfo &deviceInfo) {
    return true;
}

template <typename Family>
inline void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {
}

template <typename Family>
void EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.indirectObjectBaseAddress = sbaCmd.getIndirectObjectBaseAddress();
    sbaAddress.bindlessSurfaceStateBaseAddress = sbaCmd.getBindlessSurfaceStateBaseAddress();
    sbaAddress.dynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
    sbaAddress.generalStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
    sbaAddress.instructionBaseAddress = sbaCmd.getInstructionBaseAddress();
    sbaAddress.surfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(EncodeStateBaseAddressArgs<Family> &args) {
    auto &device = *args.container->getDevice();

    if (args.container->isAnyHeapDirty()) {
        EncodeWA<Family>::encodeAdditionalPipelineSelect(*args.container->getCommandStream(), {}, true, device.getRootDeviceEnvironment(), args.isRcs);
    }

    auto gmmHelper = device.getGmmHelper();

    auto dsh = args.container->isHeapDirty(HeapType::DYNAMIC_STATE) ? args.container->getIndirectHeap(HeapType::DYNAMIC_STATE) : nullptr;
    auto ioh = args.container->isHeapDirty(HeapType::INDIRECT_OBJECT) ? args.container->getIndirectHeap(HeapType::INDIRECT_OBJECT) : nullptr;
    auto ssh = args.container->isHeapDirty(HeapType::SURFACE_STATE) ? args.container->getIndirectHeap(HeapType::SURFACE_STATE) : nullptr;
    auto isDebuggerActive = device.getDebugger() != nullptr;
    uint64_t globalHeapsBase = 0;
    uint64_t bindlessSurfStateBase = 0;
    bool useGlobalSshAndDsh = false;

    if (device.getBindlessHeapsHelper()) {
        bindlessSurfStateBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
    }

    StateBaseAddressHelperArgs<Family> stateBaseAddressHelperArgs = {
        0,                                                  // generalStateBaseAddress
        args.container->getIndirectObjectHeapBaseAddress(), // indirectObjectHeapBaseAddress
        args.container->getInstructionHeapBaseAddress(),    // instructionHeapBaseAddress
        globalHeapsBase,                                    // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        bindlessSurfStateBase,                              // bindlessSurfaceStateBaseAddress
        &args.sbaCmd,                                       // stateBaseAddressCmd
        args.sbaProperties,                                 // sbaProperties
        dsh,                                                // dsh
        ioh,                                                // ioh
        ssh,                                                // ssh
        gmmHelper,                                          // gmmHelper
        args.statelessMocsIndex,                            // statelessMocsIndex
        args.l1CachePolicy,                                 // l1CachePolicy
        args.l1CachePolicyDebuggerActive,                   // l1CachePolicyDebuggerActive
        NEO::MemoryCompressionState::NotApplicable,         // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        false,                                              // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                                 // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        args.useGlobalAtomics,                              // useGlobalAtomics
        false,                                              // areMultipleSubDevicesInContext
        false,                                              // overrideSurfaceStateBaseAddress
        isDebuggerActive,                                   // isDebuggerActive
        args.doubleSbaWa                                    // doubleSbaWa
    };

    StateBaseAddressHelper<Family>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs,
                                                                             *args.container->getCommandStream());

    EncodeWA<Family>::encodeAdditionalPipelineSelect(*args.container->getCommandStream(), {}, false, device.getRootDeviceEnvironment(), args.isRcs);
}

template <typename Family>
size_t EncodeStateBaseAddress<Family>::getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container, bool isRcs) {
    return sizeof(typename Family::STATE_BASE_ADDRESS) + 2 * EncodeWA<Family>::getAdditionalPipelineSelectSize(device, isRcs);
}

template <typename Family>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
    auto offset = L3CNTLRegisterOffset<Family>::registerOffset;
    auto data = PreambleHelper<Family>::getL3Config(container.getDevice()->getHardwareInfo(), enableSLM);
    EncodeSetMMIO<Family>::encodeIMM(container, offset, data, false);
}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args, bool is3DPipeline,
                                                                const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {}

template <typename GfxFamily>
inline size_t EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(Device &device, bool isRcs) {
    return 0;
}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                                               const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStream, args);
}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream,
                                                                      const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlushRequired) {
    PipeControlArgs args;
    args.dcFlushEnable = dcFlushRequired;
    args.textureCacheInvalidationEnable = true;

    NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, rootDeviceEnvironment, isRcs);
}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
}

template <typename GfxFamily>
inline void EncodeSurfaceState<GfxFamily>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    encodeExtraCacheSettings(surfaceState, args);
}

template <typename GfxFamily>
bool EncodeSurfaceState<GfxFamily>::isBindingTablePrefetchPreferred() {
    return true;
}

template <typename Family>
inline void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
    surfaceState->setCoherencyType(coherencyType);
}

template <typename Family>
void EncodeSemaphore<Family>::programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                                     uint64_t compareAddress,
                                                     uint64_t compareData,
                                                     COMPARE_OPERATION compareMode,
                                                     bool registerPollMode,
                                                     bool waitMode,
                                                     bool useQwordData,
                                                     bool indirect) {
    constexpr uint64_t upper32b = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) << 32;
    UNRECOVERABLE_IF(useQwordData || (compareData & upper32b));
    UNRECOVERABLE_IF(indirect);

    MI_SEMAPHORE_WAIT localCmd = Family::cmdInitMiSemaphoreWait;
    localCmd.setCompareOperation(compareMode);
    localCmd.setSemaphoreDataDword(static_cast<uint32_t>(compareData));
    localCmd.setSemaphoreGraphicsAddress(compareAddress);
    localCmd.setWaitMode(waitMode ? MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE : MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_SIGNAL_MODE);

    *cmd = localCmd;
}

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(LinearStream &commandStream, uint64_t backBuffer) {
}

template <typename Family>
inline void EncodeStoreMemory<Family>::programStoreDataImm(MI_STORE_DATA_IMM *cmdBuffer,
                                                           uint64_t gpuAddress,
                                                           uint32_t dataDword0,
                                                           uint32_t dataDword1,
                                                           bool storeQword,
                                                           bool workloadPartitionOffset) {
    MI_STORE_DATA_IMM storeDataImmediate = Family::cmdInitStoreDataImm;
    storeDataImmediate.setAddress(gpuAddress);
    storeDataImmediate.setStoreQword(storeQword);
    storeDataImmediate.setDataDword0(dataDword0);
    if (storeQword) {
        storeDataImmediate.setDataDword1(dataDword1);
        storeDataImmediate.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD);
    } else {
        storeDataImmediate.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
    }
    *cmdBuffer = storeDataImmediate;
}

template <typename Family>
inline void EncodeMiArbCheck<Family>::adjust(MI_ARB_CHECK &miArbCheck, std::optional<bool> preParserDisable) {
}

template <typename Family>
void EncodeDispatchKernel<Family>::setupPostSyncMocs(WALKER_TYPE &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush) {}

template <typename Family>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WALKER_TYPE &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <typename Family>
size_t EncodeDispatchKernel<Family>::additionalSizeRequiredDsh(uint32_t iddCount) {
    return iddCount * sizeof(typename Family::INTERFACE_DESCRIPTOR_DATA);
}

template <typename Family>
size_t EncodeStates<Family>::getSshHeapSize() {
    return 64 * KB;
}

} // namespace NEO
