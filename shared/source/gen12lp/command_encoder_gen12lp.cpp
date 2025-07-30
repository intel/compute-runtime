/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_from_gen12lp_to_xe2_hpg.inl"
#include "shared/source/command_container/command_encoder_gen12lp_and_xe_hpg.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/reg_configs.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include "encode_surface_state_args.h"

#include <algorithm>

using Family = NEO::Gen12LpFamily;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount,
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
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = args.dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = args.dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = args.dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto pImplicitArgs = args.dispatchInterface->getImplicitArgs();

    auto &hwInfo = args.device->getHardwareInfo();
    auto &rootDeviceEnvironment = args.device->getRootDeviceEnvironment();

    LinearStream *listCmdBufferStream = container.getCommandStream();

    auto threadGroupDims = static_cast<const uint32_t *>(args.threadGroupDimensions);

    DefaultWalkerType cmd = Family::cmdInitGpgpuWalker;
    auto idd = Family::cmdInitInterfaceDescriptorData;
    {
        auto alloc = args.dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch() + args.dispatchInterface->getIsaOffsetInParentAllocation();
        idd.setKernelStartPointer(offset);
        idd.setKernelStartPointerHigh(0u);
    }

    if (args.dispatchInterface->getKernelDescriptor().kernelAttributes.flags.usesAssert && args.device->getL0Debugger() != nullptr) {
        idd.setSoftwareExceptionEnable(1);
    }

    auto numThreadsPerThreadGroup = args.dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(numThreadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor,
                                                       hwInfo);
    auto slmSize = EncodeDispatchKernel<Family>::computeSlmValues(hwInfo, args.dispatchInterface->getSlmTotalSize(), nullptr, false);
    idd.setSharedLocalMemorySize(slmSize);

    uint32_t bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    uint32_t bindingTablePointer = 0u;
    bool isBindlessKernel = NEO::KernelDescriptor::isBindlessAddressingKernel(kernelDescriptor);

    if (!isBindlessKernel) {
        container.prepareBindfulSsh();
        if (bindingTableStateCount > 0u) {
            auto ssh = args.surfaceStateHeap;
            if (ssh == nullptr) {
                ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, args.dispatchInterface->getSurfaceStateHeapDataSize(), NEO::EncodeDispatchKernel<Family>::getDefaultSshAlignment());
            }
            bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                *ssh,
                args.dispatchInterface->getSurfaceStateHeapData(),
                args.dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                kernelDescriptor.payloadMappings.bindingTable.tableOffset));
        }
    } else {
        bool globalBindlessSsh = args.device->getBindlessHeapsHelper() != nullptr;
        auto sshHeapSize = args.dispatchInterface->getSurfaceStateHeapDataSize();

        if (sshHeapSize > 0u) {
            auto ssh = args.surfaceStateHeap;
            if (ssh == nullptr) {
                container.prepareBindfulSsh();
                ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, sshHeapSize, NEO::EncodeDispatchKernel<Family>::getDefaultSshAlignment());
            }
            uint64_t bindlessSshBaseOffset = ptrDiff(ssh->getSpace(0), ssh->getCpuBase());
            if (globalBindlessSsh) {
                bindlessSshBaseOffset += ptrDiff(ssh->getGraphicsAllocation()->getGpuAddress(), ssh->getGraphicsAllocation()->getGpuBaseAddress());
            }

            DEBUG_BREAK_IF(bindingTableStateCount > 0u);

            // Allocate space for new ssh data
            auto dstSurfaceState = ssh->getSpace(sshHeapSize);
            memcpy_s(dstSurfaceState, sshHeapSize, args.dispatchInterface->getSurfaceStateHeapData(), sshHeapSize);

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
            dsHeap = container.getIndirectHeap(HeapType::dynamicState);
            auto dshSizeRequired = NEO::EncodeDispatchKernel<Family>::getSizeRequiredDsh(kernelDescriptor, container.getNumIddPerBlock());
            if (dsHeap->getAvailableSpace() <= dshSizeRequired) {
                dsHeap = container.getHeapWithRequiredSizeAndAlignment(HeapType::dynamicState, dsHeap->getMaxAvailableSpace(), NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
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
    uint32_t sizeForImplicitArgsPatching = NEO::ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, isHwLocalIdGeneration, rootDeviceEnvironment);
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching;
    uint64_t offsetThreadData = 0u;
    {
        auto heapIndirect = container.getIndirectHeap(HeapType::indirectObject);
        UNRECOVERABLE_IF(!(heapIndirect));
        heapIndirect->align(Family::cacheLineSize);
        void *ptr = nullptr;
        if (args.isKernelDispatchedFromImmediateCmdList) {
            ptr = container.getHeapWithRequiredSizeAndAlignment(HeapType::indirectObject, iohRequiredSize, DefaultWalkerType::INDIRECTDATASTARTADDRESS_ALIGN_SIZE)->getSpace(iohRequiredSize);
        } else {
            ptr = container.getHeapSpaceAllowGrow(HeapType::indirectObject, iohRequiredSize);
        }
        UNRECOVERABLE_IF(!(ptr));
        offsetThreadData = heapIndirect->getHeapGpuStartOffset() + static_cast<uint64_t>(heapIndirect->getUsed() - sizeThreadData);

        uint64_t implicitArgsGpuVA = 0u;
        if (pImplicitArgs) {
            implicitArgsGpuVA = heapIndirect->getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(heapIndirect->getUsed() - iohRequiredSize);
            auto implicitArgsCrossThreadPtr = ptrOffset(const_cast<uint64_t *>(reinterpret_cast<const uint64_t *>(args.dispatchInterface->getCrossThreadData())), kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
            *implicitArgsCrossThreadPtr = implicitArgsGpuVA;

            ptr = NEO::ImplicitArgsHelper::patchImplicitArgs(ptr, *pImplicitArgs, kernelDescriptor, {}, rootDeviceEnvironment, nullptr);
        }

        memcpy_s(ptr, sizeCrossThreadData,
                 args.dispatchInterface->getCrossThreadData(), sizeCrossThreadData);

        if (args.isIndirect) {
            auto crossThreadDataGpuVA = heapIndirect->getGraphicsAllocation()->getGpuAddress() + heapIndirect->getUsed() - sizeThreadData;
            EncodeIndirectParams<Family>::encode(container, crossThreadDataGpuVA, args.dispatchInterface, implicitArgsGpuVA, nullptr);
        }

        ptr = ptrOffset(ptr, sizeCrossThreadData);
        memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                 args.dispatchInterface->getPerThreadData(), sizePerThreadDataForWholeGroup);
    }

    uint32_t numIDD = 0u;
    void *iddPtr = EncodeDispatchKernel::getInterfaceDescriptor(container, args.dynamicStateHeap, numIDD);

    auto slmSizeNew = args.dispatchInterface->getSlmTotalSize();
    bool dirtyHeaps = container.isAnyHeapDirty();
    bool flush = container.slmSizeRef() != slmSizeNew || dirtyHeaps || args.requiresUncachedMocs;

    if (flush) {
        PipeControlArgs syncArgs;
        syncArgs.dcFlushEnable = args.postSyncArgs.dcFlushEnable;
        if (dirtyHeaps) {
            syncArgs.hdcPipelineFlush = true;
        }
        MemorySynchronizationCommands<Family>::addSingleBarrier(*container.getCommandStream(), syncArgs);

        if (dirtyHeaps || args.requiresUncachedMocs) {
            STATE_BASE_ADDRESS sba;
            auto gmmHelper = container.getDevice()->getGmmHelper();
            uint32_t statelessMocsIndex =
                args.requiresUncachedMocs ? (gmmHelper->getUncachedMOCS() >> 1) : (gmmHelper->getL3EnabledMOCS() >> 1);
            auto l1CachePolicy = container.l1CachePolicyDataRef()->getL1CacheValue(false);
            auto l1CachePolicyDebuggerActive = container.l1CachePolicyDataRef()->getL1CacheValue(true);
            EncodeStateBaseAddressArgs<Family> encodeStateBaseAddressArgs = {
                &container,                  // container
                sba,                         // sbaCmd
                nullptr,                     // sbaProperties
                statelessMocsIndex,          // statelessMocsIndex
                l1CachePolicy,               // l1CachePolicy
                l1CachePolicyDebuggerActive, // l1CachePolicyDebuggerActive
                false,                       // multiOsContextCapable
                args.isRcs,                  // isRcs
                container.doubleSbaWaRef(),  // doubleSbaWa
                false,                       // heaplessModeEnabled
            };
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
                                                   threadGroupDims,
                                                   args.dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   numThreadsPerThreadGroup,
                                                   args.dispatchInterface->getThreadExecutionMask(),
                                                   true,
                                                   false,
                                                   args.isIndirect,
                                                   args.dispatchInterface->getRequiredWorkgroupOrder(),
                                                   rootDeviceEnvironment);

    cmd.setPredicateEnable(args.isPredicate);

    auto threadGroupCount = cmd.getThreadGroupIdXDimension() * cmd.getThreadGroupIdYDimension() * cmd.getThreadGroupIdZDimension();
    EncodeDispatchKernel<Family>::encodeThreadGroupDispatch(idd, *args.device, hwInfo, threadGroupDims, threadGroupCount, 0, kernelDescriptor.kernelAttributes.numGrfRequired, numThreadsPerThreadGroup, cmd);

    EncodeWalkerArgs walkerArgs{
        .kernelExecutionType = KernelExecutionType::defaultType,
        .requiredDispatchWalkOrder = args.requiredDispatchWalkOrder,
        .localRegionSize = args.localRegionSize,
        .maxFrontEndThreads = args.device->getDeviceInfo().maxFrontEndThreads,
        .requiredSystemFence = args.postSyncArgs.requiresSystemMemoryFence(),
        .hasSample = false};

    using INTERFACE_DESCRIPTOR_DATA = typename Family::INTERFACE_DESCRIPTOR_DATA;
    EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(rootDeviceEnvironment, cmd, walkerArgs);
    EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields(cmd, rootDeviceEnvironment, walkerArgs);
    EncodeDispatchKernel<Family>::template encodeComputeDispatchAllWalker<WalkerType, INTERFACE_DESCRIPTOR_DATA>(cmd, nullptr, rootDeviceEnvironment, walkerArgs);

    memcpy_s(iddPtr, sizeof(idd), &idd, sizeof(idd));

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(args.device->getRootDeviceEnvironment(), NEO::PostSyncMode::noWrite));
        args.additionalCommands->push_back(commandBuffer);

        EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
    }

    auto buffer = listCmdBufferStream->getSpaceForCmd<DefaultWalkerType>();
    *buffer = cmd;

    {
        auto mediaStateFlush = listCmdBufferStream->getSpaceForCmd<MEDIA_STATE_FLUSH>();
        *mediaStateFlush = Family::cmdInitMediaStateFlush;
    }

    args.partitionCount = 1;

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(args.device->getRootDeviceEnvironment(), NEO::PostSyncMode::noWrite));
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
        heapBase = container.getIndirectHeap(HeapType::dynamicState)->getCpuBase();
    }

    auto mediaStateFlush = container.getCommandStream()->getSpaceForCmd<MEDIA_STATE_FLUSH>();
    *mediaStateFlush = Family::cmdInitMediaStateFlush;

    auto iddOffset = static_cast<uint32_t>(ptrDiff(container.getIddBlock(), heapBase));

    MEDIA_INTERFACE_DESCRIPTOR_LOAD cmd = Family::cmdInitMediaInterfaceDescriptorLoad;
    cmd.setInterfaceDescriptorDataStartAddress(iddOffset);
    using INTERFACE_DESCRIPTOR_DATA = typename Family::INTERFACE_DESCRIPTOR_DATA;
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
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeThreadData(WalkerType &walkerCmd,
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

    walkerCmd.setSimdSize(getSimdConfig<WalkerType>(simd));

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
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::programBarrierEnable(InterfaceDescriptorType &interfaceDescriptor,
                                                        const KernelDescriptor &kernelDescriptor,
                                                        const HardwareInfo &hwInfo) {
    interfaceDescriptor.setBarrierEnable(kernelDescriptor.kernelAttributes.barrierCount);
}

template <typename Family>
template <typename WalkerType>
inline void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
template <typename WalkerType>
inline void EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields(WalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
inline void EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setupPreferredSlmSize(InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {}

template <typename Family>
inline bool EncodeDispatchKernel<Family>::isDshNeeded(const DeviceInfo &deviceInfo) {
    return true;
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

    auto dsh = args.container->isHeapDirty(HeapType::dynamicState) ? args.container->getIndirectHeap(HeapType::dynamicState) : nullptr;
    auto ioh = args.container->isHeapDirty(HeapType::indirectObject) ? args.container->getIndirectHeap(HeapType::indirectObject) : nullptr;
    auto ssh = args.container->isHeapDirty(HeapType::surfaceState) ? args.container->getIndirectHeap(HeapType::surfaceState) : nullptr;
    auto isDebuggerActive = device.getDebugger() != nullptr;
    uint64_t globalHeapsBase = 0;
    uint64_t bindlessSurfStateBase = 0;
    bool useGlobalSshAndDsh = false;

    if (device.getBindlessHeapsHelper()) {
        bindlessSurfStateBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        globalHeapsBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        useGlobalSshAndDsh = true;
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
        NEO::MemoryCompressionState::notApplicable,         // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        false,                                              // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                                 // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        false,                                              // areMultipleSubDevicesInContext
        false,                                              // overrideSurfaceStateBaseAddress
        isDebuggerActive,                                   // isDebuggerActive
        args.doubleSbaWa,                                   // doubleSbaWa
        args.heaplessModeEnabled                            // heaplessModeEnabled
    };

    StateBaseAddressHelper<Family>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs,
                                                                             *args.container->getCommandStream());

    EncodeWA<Family>::encodeAdditionalPipelineSelect(*args.container->getCommandStream(), {}, false, device.getRootDeviceEnvironment(), args.isRcs);
}

template <typename Family>
size_t EncodeStateBaseAddress<Family>::getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container, bool isRcs) {
    return sizeof(typename Family::STATE_BASE_ADDRESS) + 2 * EncodeWA<Family>::getAdditionalPipelineSelectSize(device, isRcs);
}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                                               const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStream, args);
}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
}

template <typename Family>
void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
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
                                                     bool indirect,
                                                     bool switchOnUnsuccessful) {
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
    EncodeStoreMemory<Family>::encodeForceCompletionCheck(storeDataImmediate);

    *cmdBuffer = storeDataImmediate;
}

template <typename Family>
uint32_t EncodePostSync<Family>::getPostSyncMocs(const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush) {
    return 0;
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setupPostSyncForRegularEvent(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::encodeL3Flush(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setupPostSyncForInOrderExec(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <typename Family>
size_t EncodeDispatchKernel<Family>::additionalSizeRequiredDsh(uint32_t iddCount) {
    return iddCount * sizeof(typename Family::INTERFACE_DESCRIPTOR_DATA);
}

template <typename Family>
inline size_t EncodeDispatchKernel<Family>::getInlineDataOffset(EncodeDispatchKernelArgs &args) {
    return 0;
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::forceComputeWalkerPostSyncFlushWithWrite(WalkerType &walkerCmd) {
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 1024u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    UNRECOVERABLE_IF(slmSize > 64u * MemoryConstants::kiloByte);
    return slmSize;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize, ReleaseHelper *releaseHelper, bool isHeapless) {
    auto value = std::max(slmSize, 1024u);
    value = Math::nextPowerOfTwo(value);
    value = Math::getMinLsbSet(value);
    value = value - 9;
    DEBUG_BREAK_IF(value > 7);
    return value * !!slmSize;
}

template <typename Family>
bool EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) {
    return cooperativeKernel;
}

template <typename Family>
size_t EncodeStates<Family>::getSshHeapSize() {
    return 64 * MemoryConstants::kiloByte;
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue) {
    UNRECOVERABLE_IF(true);
}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::overrideDefaultValues(WalkerType &walkerCmd, InterfaceDescriptorType &interfaceDescriptor) {
}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeThreadGroupDispatch(InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo,
                                                             const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t requiredThreadGroupDispatchSize,
                                                             const uint32_t grfCount, const uint32_t threadsPerThreadGroup, WalkerType &walkerCmd) {
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::getScratchPtrOffsetOfImplicitArgs() {
    return 0;
}

template <typename Family>
void EncodeSurfaceState<Family>::setPitchForScratch(R_SURFACE_STATE *surfaceState, uint32_t pitch, const ProductHelper &productHelper) {
    surfaceState->setSurfacePitch(pitch);
}

template <typename Family>
uint32_t EncodeSurfaceState<Family>::getPitchForScratchInBytes(R_SURFACE_STATE *surfaceState, const ProductHelper &productHelper) {
    return surfaceState->getSurfacePitch();
}

template <typename Family>
void EncodeSurfaceState<Family>::convertSurfaceStateToPacked(R_SURFACE_STATE *surfaceState, ImageInfo &imgInfo) {
}

template <typename Family>
void EncodeSemaphore<Family>::appendSemaphoreCommand(MI_SEMAPHORE_WAIT &cmd, uint64_t compareData, bool indirect, bool useQwordData, bool switchOnUnsuccessful) {
    constexpr uint64_t upper32b = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) << 32;
    UNRECOVERABLE_IF(useQwordData || (compareData & upper32b));
}

template <typename Family>
template <bool isHeapless>
void EncodeDispatchKernel<Family>::setScratchAddress(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr) {
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setWalkerRegionSettings(WalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount, uint32_t workgroupSize, uint32_t threadGroupCount, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder) {}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::adjustTimestampPacket(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType) {}

template <>
size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device, bool isRcs) {
    size_t size = 0;
    const auto &productHelper = device.getProductHelper();
    if (isRcs && productHelper.is3DPipelineSelectWARequired()) {
        size += 2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(device.getRootDeviceEnvironment());
    }
    return size;
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    FORCE_NON_COHERENT coherencyValue = (properties.isCoherencyRequired.value == 1) ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED
                                                                                    : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    stateComputeMode.setForceNonCoherent(coherencyValue);
    maskBits |= Family::stateComputeModeForceNonCoherentMask;

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpace(sizeof(STATE_COMPUTE_MODE));
    *reinterpret_cast<STATE_COMPUTE_MODE *>(buffer) = stateComputeMode;
}

template <>
void EncodeWA<Family>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args,
                                                      bool is3DPipeline, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        PipelineSelectArgs pipelineSelectArgs = args;
        pipelineSelectArgs.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<Family>::programPipelineSelect(&stream, pipelineSelectArgs, rootDeviceEnvironment);
    }
}

template <>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    const bool isL3Allowed = surfaceState->getMemoryObjectControlState() == args.gmmHelper->getL3EnabledMOCS();
    if (isL3Allowed) {
        const bool isConstantSurface = args.allocation && args.allocation->getAllocationType() == AllocationType::constantSurface;
        bool useL1 = args.isReadOnly || isConstantSurface;

        if (debugManager.flags.ForceL1Caching.get() != 1) {
            useL1 = false;
        }

        if (useL1) {
            surfaceState->setMemoryObjectControlState(args.gmmHelper->getL1EnabledMOCS());
        }
    }
}

template <>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
}

template <>
void EncodeStoreMMIO<Family>::appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition) {
    storeRegMem->setMmioRemapEnable(true);
}

template <>
void EncodeSurfaceState<Family>::appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation,
                                                              GmmHelper *gmmHelper, bool imageFromBuffer, GMM_YUV_PLANE_ENUM plane) {
}

template <>
inline void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const EncodeSurfaceStateArgs &args) {}

template <>
inline void EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {}

template <>
bool EncodeEnableRayTracing<Family>::is48bResourceNeededForRayTracing() {
    return true;
}

template <>
void EncodeDataMemory<Family>::programFrontEndState(
    LinearStream &commandStream,
    uint64_t dstGpuAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment,
    uint32_t scratchSize,
    uint64_t scratchAddress,
    uint32_t maxFrontEndThreads,
    const StreamProperties &streamProperties) {
}

template <>
void EncodeDataMemory<Family>::programFrontEndState(
    void *&commandBuffer,
    uint64_t dstGpuAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment,
    uint32_t scratchSize,
    uint64_t scratchAddress,
    uint32_t maxFrontEndThreads,
    const StreamProperties &streamProperties) {
}

template <typename Family>
void EncodeSurfaceState<Family>::setAdditionalCacheSettings(R_SURFACE_STATE *surfaceState) {
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template struct EncodeL3State<Family>;

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType);
template struct EncodeDispatchKernelWithHeap<Family>;
template void NEO::EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &, unsigned int, unsigned int);

} // namespace NEO

#include "shared/source/command_container/implicit_scaling_before_xe_hp.inl"
