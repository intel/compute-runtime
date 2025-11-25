/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

namespace NEO {
template <typename Family, typename WalkerType>
uint32_t getQuantumSizeHw(uint32_t selectedQuantumSize);
}

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_from_xe3_and_later.inl"
#include "shared/source/command_container/command_encoder_from_xe3p_and_later.inl"
#include "shared/source/command_container/command_encoder_heapless_addressing.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe2_hpg_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpc_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"

#include "implicit_args.h"

using Family = NEO::Xe3pCoreFamily;
#include <algorithm>
#include <type_traits>

namespace NEO {

template <>
void EncodeDispatchKernel<Family>::encodeCommon(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    if (args.isHeaplessModeEnabled) {
        EncodeDispatchKernel<Family>::template encode<typename Family::COMPUTE_WALKER_2>(container, args);
    } else {
        EncodeDispatchKernel<Family>::template encode<typename Family::COMPUTE_WALKER>(container, args);
    }
}

template <>
size_t EncodeDispatchKernel<Family>::getInlineDataOffset(EncodeDispatchKernelArgs &args) {
    if (args.isHeaplessModeEnabled) {
        using WalkerType = typename Family::COMPUTE_WALKER_2;
        return offsetof(WalkerType, TheStructure.Common.InlineData);
    } else {
        using WalkerType = typename Family::COMPUTE_WALKER;
        return offsetof(WalkerType, TheStructure.Common.InlineData);
    }
}

template <>
uint32_t getQuantumSizeHw<Family, Family::COMPUTE_WALKER_2>(uint32_t selectedQuantumSize) {
    constexpr uint32_t quantumDisabled = 0;
    uint32_t quantumSize = quantumDisabled;

    using QUANTUMSIZE = typename Family::COMPUTE_WALKER_2::QUANTUMSIZE;

    static_assert(quantumDisabled == static_cast<uint32_t>(QUANTUMSIZE::QUANTUMSIZE_QUANTUM_DISPATCH_DISABLED), "Quantum size disabled value mismatch");

    switch (selectedQuantumSize) {
    case 4:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_4;
        break;
    case 8:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_8;
        break;
    case 14:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_14;
        break;
    case 16:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_16;
        break;
    case 20:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_20;
        break;
    case 24:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_24;
        break;
    case 28:
        quantumSize = QUANTUMSIZE::QUANTUMSIZE_QUANTUM_SIZE_28;
        break;
    default:
        UNRECOVERABLE_IF(selectedQuantumSize != NEO::localRegionSizeParamNotSet);
        break;
    }

    return quantumSize;
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using LSC_SAMPLER_BACKING_THRESHOLD = typename STATE_COMPUTE_MODE::LSC_SAMPLER_BACKING_THRESHOLD;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMask1();
    auto maskBits2 = stateComputeMode.getMask2();

    if (properties.enablePageFaultException.isDirty) {
        stateComputeMode.setPageFaultExceptionEnable(properties.enablePageFaultException.value);
        maskBits2 |= Family::stateComputeModePageFaultExceptionEnableMask;
    }

    if (properties.enableSystemMemoryReadFence.isDirty) {
        stateComputeMode.setSystemMemoryReadFenceEnable(properties.enableSystemMemoryReadFence.value);
        maskBits2 |= Family::stateComputeModeSystemMemoryReadFenceEnableMask;
    }

    if (properties.enableMemoryException.isDirty) {
        stateComputeMode.setMemoryExceptionEnable(properties.enableMemoryException.value);
        maskBits2 |= Family::stateComputeModeMemoryExceptionEnableMask;
    }

    if (properties.enableBreakpoints.isDirty) {
        stateComputeMode.setEnableBreakpoints(properties.enableBreakpoints.value);
        maskBits2 |= Family::stateComputeBreakpointsEnableMask;
    }

    if (properties.enableForceExternalHaltAndForceException.isDirty) {
        stateComputeMode.setEnableForceExternalHaltAndForceException(properties.enableForceExternalHaltAndForceException.value);
        maskBits2 |= Family::stateComputeModeForceExternalHaltAndForceExceptionEnableMask;
    }

    if (properties.isPipelinedEuThreadArbitrationEnabled()) {
        if (properties.pipelinedEuThreadArbitration.isDirty) {
            stateComputeMode.setEnablePipelinedEuThreadArbitration(true);
            maskBits |= Family::stateComputeModePipelinedEuThreadArbitrationMask;
        }
    } else if (properties.threadArbitrationPolicy.isDirty) {
        switch (properties.threadArbitrationPolicy.value) {
        case ThreadArbitrationPolicy::RoundRobin:
            stateComputeMode.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN);
            break;
        case ThreadArbitrationPolicy::AgeBased:
            stateComputeMode.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST);
            break;
        case ThreadArbitrationPolicy::RoundRobinAfterDependency:
            stateComputeMode.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
            break;
        default:
            stateComputeMode.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT);
        }
        maskBits |= Family::stateComputeModeEuThreadSchedulingModeOverrideMask;
    }

    if (properties.largeGrfMode.isDirty) {
        stateComputeMode.setLargeGrfMode(properties.largeGrfMode.value);
        maskBits |= Family::stateComputeModeLargeGrfModeMask;
    }

    if (properties.enableVariableRegisterSizeAllocation.isDirty) {
        stateComputeMode.setEnableVariableRegisterSizeAllocationVrt(properties.enableVariableRegisterSizeAllocation.value);
        maskBits |= Family::stateComputeModeEnableVariableRegisterSizeAllocationMask;
    }

    if (properties.enableL1FlushUavCoherencyMode.isDirty) {
        stateComputeMode.setUavCoherencyMode(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE::UAV_COHERENCY_MODE_FLUSH_DATAPORT_L1);
        maskBits2 |= Family::stateComputeModeUavCoherencyModeMask;
    }

    if (properties.enableOutOfBoundariesInTranslationException.isDirty) {
        stateComputeMode.setOutOfBoundariesInTranslationExceptionEnable(properties.enableOutOfBoundariesInTranslationException.value);
        maskBits2 |= Family::stateComputeModeEnableOutOfBoundariesInTranslationExceptionMask;
    }

    if (properties.lscSamplerBackingThreshold.isDirty) {
        auto thresholdLevel = static_cast<LSC_SAMPLER_BACKING_THRESHOLD>(properties.lscSamplerBackingThreshold.value);
        stateComputeMode.setLscSamplerBackingThreshold(thresholdLevel);
        maskBits |= Family::stateComputeModeLSCSamplerBackingThresholdMask;
    }

    stateComputeMode.setMask1(maskBits);
    stateComputeMode.setMask2(maskBits2);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
uint32_t EncodeDispatchKernel<Family>::alignSlmSize(uint32_t slmSize) {
    const uint32_t alignedSlmSizes[] = {
        0u,
        1u * MemoryConstants::kiloByte,
        2u * MemoryConstants::kiloByte,
        4u * MemoryConstants::kiloByte,
        8u * MemoryConstants::kiloByte,
        16u * MemoryConstants::kiloByte,
        24u * MemoryConstants::kiloByte,
        32u * MemoryConstants::kiloByte,
        48u * MemoryConstants::kiloByte,
        64u * MemoryConstants::kiloByte,
        96u * MemoryConstants::kiloByte,
        128u * MemoryConstants::kiloByte,
        192u * MemoryConstants::kiloByte,
        256u * MemoryConstants::kiloByte,
        320u * MemoryConstants::kiloByte,
        384u * MemoryConstants::kiloByte,
    };

    for (auto &alignedSlmSize : alignedSlmSizes) {
        if (slmSize <= alignedSlmSize) {
            return alignedSlmSize;
        }
    }

    UNRECOVERABLE_IF(true);
    return 0;
}

template <>
uint32_t EncodeDispatchKernel<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize, ReleaseHelper *releaseHelper, bool isHeapless) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    if (slmSize == 0u) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;
    }
    UNRECOVERABLE_IF(!releaseHelper);
    return releaseHelper->computeSlmValues(slmSize, isHeapless);
}

template <>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::overrideDefaultValues(WalkerType &walkerCmd, InterfaceDescriptorType &interfaceDescriptor) {
    using DYNAMIC_PREF_SLM_INCREASE = InterfaceDescriptorType::DYNAMIC_PREF_SLM_INCREASE;

    auto dynamicPerfSlmSizeCtrl = DYNAMIC_PREF_SLM_INCREASE::DYNAMIC_PREF_SLM_INCREASE_MAX_FULL;

    if (debugManager.flags.OverrideDynamicPrefSlmIncrease.get() != -1) {
        dynamicPerfSlmSizeCtrl = static_cast<DYNAMIC_PREF_SLM_INCREASE>(debugManager.flags.OverrideDynamicPrefSlmIncrease.get());
    }

    interfaceDescriptor.setDynamicPrefSlmIncrease(dynamicPerfSlmSizeCtrl);

    constexpr bool heaplessModeEnabled = Family::isHeaplessMode<WalkerType>();
    if constexpr (heaplessModeEnabled) {
        if (debugManager.flags.OverDispatchControl.get() != -1) {
            walkerCmd.setOverDispatchControl(static_cast<typename WalkerType::OVER_DISPATCH_CONTROL>(debugManager.flags.OverDispatchControl.get()));
        }

        bool disableOverdispatch = true;
        if (debugManager.flags.ComputeOverdispatchDisable.get() != -1) {
            disableOverdispatch = static_cast<bool>(debugManager.flags.ComputeOverdispatchDisable.get());
        }
        walkerCmd.setComputeOverdispatchDisable(disableOverdispatch);

        if (debugManager.flags.OverrideComputeWalker2ThreadArbitrationPolicy.get() != -1) {
            using THREAD_ARBITRATION_POLICY = typename Family::COMPUTE_WALKER_2::THREAD_ARBITRATION_POLICY;
            walkerCmd.setThreadArbitrationPolicy(static_cast<THREAD_ARBITRATION_POLICY>(debugManager.flags.OverrideComputeWalker2ThreadArbitrationPolicy.get()));
        }
    } else {
        int32_t forceL3PrefetchForComputeWalker = debugManager.flags.ForceL3PrefetchForComputeWalker.get();
        if (forceL3PrefetchForComputeWalker != -1) {
            walkerCmd.setL3PrefetchDisable(!forceL3PrefetchForComputeWalker);
        }
    }
}

template <typename Family>
template <typename PostSyncT>
void EncodePostSync<Family>::setPostSyncData(PostSyncT &postSyncData, typename PostSyncT::OPERATION operation, uint64_t gpuVa, uint64_t immData,
                                             [[maybe_unused]] uint32_t atomicOpcode, uint32_t mocs, [[maybe_unused]] bool interrupt, bool requiresSystemMemoryFence) {
    using POSTSYNC_DATA_2 = typename Family::POSTSYNC_DATA_2;

    setPostSyncDataCommon(postSyncData, operation, gpuVa, immData);
    postSyncData.setDataportPipelineFlush(true);
    postSyncData.setDataportSubsliceCacheFlush(true);

    if (NEO::debugManager.flags.ForcePostSyncL1Flush.get() != -1) {
        postSyncData.setDataportPipelineFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
        postSyncData.setDataportSubsliceCacheFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
    }

    postSyncData.setMocs(mocs);
    postSyncData.setSystemMemoryFenceRequest(requiresSystemMemoryFence);

    if constexpr (std::is_same_v<PostSyncT, POSTSYNC_DATA_2>) {
        postSyncData.setAtomicOpcode(static_cast<typename POSTSYNC_DATA_2::ATOMIC_OPCODE>(atomicOpcode)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        postSyncData.setAtomicDataSize(POSTSYNC_DATA_2::ATOMIC_DATA_SIZE_QWORD);
        postSyncData.setInterruptSignalEnable(interrupt);

        postSyncData.setSerializePostsyncOps(debugManager.flags.SerializeWalkerPostSyncOps.get() == 1);
    }
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::adjustTimestampPacket(CommandType &cmd, const EncodePostSyncArgs &args) {
    if constexpr (std::is_same_v<CommandType, typename Family::COMPUTE_WALKER_2>) {
        cmd.getPostSync().setInterruptSignalEnable(args.interruptEvent);
    }
}

template <typename Family>
template <typename CommandType>
inline auto &EncodePostSync<Family>::getPostSync(CommandType &cmd, size_t index) {
    using PostSyncType = decltype(Family::template getPostSyncType<CommandType>());
    if constexpr (std::is_same_v<PostSyncType, typename Family::POSTSYNC_DATA>) {
        DEBUG_BREAK_IF(index != 0);
        return cmd.getPostSync();
    } else {
        static_assert(std::is_same_v<PostSyncType, typename Family::POSTSYNC_DATA_2>);
        switch (index) {
        case 0:
            return cmd.getPostSync();
        case 1:
            return cmd.getPostSyncOpn1();
        case 2:
            return cmd.getPostSyncOpn2();
        case 3:
            return cmd.getPostSyncOpn3();
        default:
            UNRECOVERABLE_IF(true);
        }
    }
}

template <typename Family>
size_t EncodeSemaphore<Family>::getSizeMiSemaphoreWait() {
    return NEO::debugManager.flags.Enable64BitSemaphore.get() == 1 ? sizeof(typename Family::MI_SEMAPHORE_WAIT_64) : sizeof(MI_SEMAPHORE_WAIT);
}

template <typename Family>
void EncodeSemaphore<Family>::addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                                        uint64_t compareAddress,
                                                        uint64_t compareData,
                                                        COMPARE_OPERATION compareMode,
                                                        bool registerPollMode,
                                                        bool useQwordData,
                                                        bool indirect,
                                                        bool switchOnUnsuccessful,
                                                        void **outSemWaitCmd) {
    void *semaphoreCommand = commandStream.getSpace(EncodeSemaphore<Family>::getSizeMiSemaphoreWait());

    if (outSemWaitCmd != nullptr) {
        *outSemWaitCmd = semaphoreCommand;
    }
    programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(semaphoreCommand), compareAddress, compareData, compareMode, registerPollMode, true, useQwordData, indirect, switchOnUnsuccessful);
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
    if (debugManager.flags.Enable64BitSemaphore.get()) {
        using MI_SEMAPHORE_WAIT_64 = typename Family::MI_SEMAPHORE_WAIT_64;
        using COMPARE_OPERATION_64 = typename MI_SEMAPHORE_WAIT_64::COMPARE_OPERATION;

        UNRECOVERABLE_IF(!waitMode) // WAIT_MODE_SIGNAL_MODE is not supported

        MI_SEMAPHORE_WAIT_64 localCmd = Family::cmdInitMiSemaphoreWait64;
        localCmd.setCompareOperation(static_cast<COMPARE_OPERATION_64>(compareMode));
        localCmd.setSemaphoreDataDword(compareData);
        localCmd.setSemaphoreGraphicsAddress(compareAddress);
        localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT_64::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT_64::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);
        localCmd.setIndirectSemaphoreDataDword(indirect);
        localCmd.set64BCompareDisable(useQwordData ? MI_SEMAPHORE_WAIT_64::_64B_COMPARE_DISABLE::_64B_COMPARE_DISABLE_64B_COMPARE : MI_SEMAPHORE_WAIT_64::_64B_COMPARE_DISABLE::_64B_COMPARE_DISABLE_32B_COMPARE);

        if (switchOnUnsuccessful) {
            localCmd.setQueueSwitchMode(MI_SEMAPHORE_WAIT_64::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL);
        }

        if (indirect && useQwordData) {
            localCmd.setSemaphoreDataDword(0);
        }

        memcpy_s(cmd, sizeof(MI_SEMAPHORE_WAIT_64), &localCmd, sizeof(MI_SEMAPHORE_WAIT_64));
    } else {
        MI_SEMAPHORE_WAIT localCmd = Family::cmdInitMiSemaphoreWait;
        localCmd.setCompareOperation(compareMode);
        localCmd.setSemaphoreDataDword(static_cast<uint32_t>(compareData));
        localCmd.setSemaphoreGraphicsAddress(compareAddress);
        localCmd.setWaitMode(waitMode ? MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE : MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_SIGNAL_MODE);
        localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);
        localCmd.setIndirectSemaphoreDataDword(indirect);

        if (indirect && useQwordData) {
            localCmd.set64bCompareEnableWithGPR(true);
            localCmd.setIndirectSemaphoreDataDword(false);
            localCmd.setSemaphoreDataDword(0);
        }

        if (switchOnUnsuccessful) {
            localCmd.setQueueSwitchMode(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL);
        }

        *cmd = localCmd;
    }
}

template <typename Family>
void EncodeSemaphore<Family>::setMiSemaphoreWaitValue(void *cmd, uint64_t semaphoreValue) {
    if (debugManager.flags.Enable64BitSemaphore.get()) {
        reinterpret_cast<Family::MI_SEMAPHORE_WAIT_64 *>(cmd)->setSemaphoreDataDword(semaphoreValue);
    } else {
        reinterpret_cast<Family::MI_SEMAPHORE_WAIT *>(cmd)->setSemaphoreDataDword(static_cast<uint32_t>(semaphoreValue));
    }
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setCommandLevelInterrupt(CommandType &cmd, bool interrupt) {}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType) {}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void NEO::EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields<Family::COMPUTE_WALKER>(const RootDeviceEnvironment &rootDeviceEnvironment, Family::COMPUTE_WALKER &walkerCmd, const EncodeWalkerArgs &walkerArgs);
template void NEO::EncodeDispatchKernel<Family>::setGrfInfo<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount, const size_t &sizeCrossThreadData, const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::setupPreferredSlmSize<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);
template void NEO::EncodeDispatchKernel<Family>::setupProgrammableSlmSize<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t slmTotalSize, bool heaplessModeEnabled);
template void NEO::EncodeDispatchKernel<Family>::encodeThreadGroupDispatch<Family::COMPUTE_WALKER, Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t requiredThreadGroupDispatchSize, const uint32_t grfCount, const uint32_t threadsPerThreadGroup, Family::COMPUTE_WALKER &walkerCmd);
template void NEO::EncodeDispatchKernel<Family>::encode<Family::COMPUTE_WALKER>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::encodeThreadData<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const uint32_t *startWorkGroup, const uint32_t *numWorkGroups, const uint32_t *workGroupSizes, uint32_t simd, uint32_t localIdDimensions, uint32_t threadsPerThreadGroup, uint32_t threadExecutionMask, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, bool isIndirect, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::adjustWalkOrder<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::programBarrierEnable<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType &interfaceDescriptor, const KernelDescriptor &kernelDescriptor, const HardwareInfo &hwInfo);
template void NEO::EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy);
template void NEO::EncodeDispatchKernel<Family>::forceComputeWalkerPostSyncFlushWithWrite<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd);
template void NEO::EncodeDispatchKernel<Family>::setWalkerRegionSettings<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const NEO::Device &device, uint32_t partitionCount,
                                                                                                 uint32_t workgroupSize, uint32_t threadGroupCount, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder);
template void NEO::EncodeDispatchKernel<Family>::overrideDefaultValues<Family::COMPUTE_WALKER, Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER &walkerCmd, Family::COMPUTE_WALKER::InterfaceDescriptorType &interfaceDescriptor);
template void NEO::EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs);
template void NEO::EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker<Family::COMPUTE_WALKER, Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER &walkerCmd, const Family::COMPUTE_WALKER::InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs);

template void NEO::EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs<true>(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrPatchingRequired);
template void NEO::EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch<Family::COMPUTE_WALKER::InterfaceDescriptorType>(Family::COMPUTE_WALKER::InterfaceDescriptorType &, unsigned int, unsigned int);

template void NEO::EncodePostSync<Family>::adjustTimestampPacket<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::setupPostSyncForRegularEvent<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::encodeL3Flush<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::setupPostSyncForInOrderExec<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, const EncodePostSyncArgs &args);

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType);

template struct EncodeDispatchKernelWithHeap<Family>;

template void NEO::EncodePostSync<Family>::setCommandLevelInterrupt<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, bool interrupt);
template void NEO::EncodePostSync<Family>::setCommandLevelInterrupt<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &cmd, bool interrupt);
} // namespace NEO
