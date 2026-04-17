/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
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
    EncodeDispatchKernel<Family>::template encode<typename Family::COMPUTE_WALKER_2>(container, args);
}

template <>
size_t EncodeDispatchKernel<Family>::getInlineDataOffset(EncodeDispatchKernelArgs &args) {

    using WalkerType = typename Family::COMPUTE_WALKER_2;
    return offsetof(WalkerType, TheStructure.Common.InlineData);
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
uint32_t EncodeDispatchKernel<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize, ReleaseHelper *releaseHelper) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    if (slmSize == 0u) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;
    }
    UNRECOVERABLE_IF(!releaseHelper);
    return releaseHelper->computeSlmValues(slmSize);
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

    postSyncData.setAtomicOpcode(static_cast<typename POSTSYNC_DATA_2::ATOMIC_OPCODE>(atomicOpcode)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    postSyncData.setAtomicDataSize(POSTSYNC_DATA_2::ATOMIC_DATA_SIZE_QWORD);
    postSyncData.setInterruptSignalEnable(interrupt);

    postSyncData.setSerializePostsyncOps(debugManager.flags.SerializeWalkerPostSyncOps.get() == 1);
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::adjustTimestampPacket(CommandType &cmd, const EncodePostSyncArgs &args) {
    cmd.getPostSync().setInterruptSignalEnable(args.interruptEvent);
}

template <typename Family>
template <typename CommandType>
inline auto &EncodePostSync<Family>::getPostSync(CommandType &cmd, size_t index) {

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

template <typename Family>
size_t EncodeSemaphore<Family>::getSizeMiSemaphoreWait() {
    static_assert(sizeof(MI_SEMAPHORE_WAIT) == sizeof(typename Family::MI_SEMAPHORE_WAIT_LEGACY), "MI_SEMAPHORE_WAIT_64/MI_SEMAPHORE_WAIT size mismatch");
    return sizeof(MI_SEMAPHORE_WAIT);
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
                                                        bool native64bCmd,
                                                        void **outSemWaitCmd) {
    void *semaphoreCommand = commandStream.getSpace(EncodeSemaphore<Family>::getSizeMiSemaphoreWait());

    if (outSemWaitCmd != nullptr) {
        *outSemWaitCmd = semaphoreCommand;
    }
    programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(semaphoreCommand), compareAddress, compareData, compareMode, registerPollMode, true, useQwordData, indirect, switchOnUnsuccessful, native64bCmd);
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
                                                     bool switchOnUnsuccessful,
                                                     bool native64bCmd) {
    if (debugManager.flags.ForceSwitchQueueOnUnsuccessful.get() != -1) {
        switchOnUnsuccessful = debugManager.flags.ForceSwitchQueueOnUnsuccessful.get() == 1;
    }

    if (native64bCmd) {
        UNRECOVERABLE_IF(!waitMode) // WAIT_MODE_SIGNAL_MODE is not supported

        MI_SEMAPHORE_WAIT localCmd = Family::cmdInitMiSemaphoreWait;
        localCmd.setCompareOperation(compareMode);
        localCmd.setSemaphoreDataDword(compareData);
        localCmd.setSemaphoreGraphicsAddress(compareAddress);
        localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);
        localCmd.setIndirectSemaphoreDataDword(indirect);
        localCmd.set64BCompareDisable(useQwordData ? MI_SEMAPHORE_WAIT::_64B_COMPARE_DISABLE::_64B_COMPARE_DISABLE_64B_COMPARE : MI_SEMAPHORE_WAIT::_64B_COMPARE_DISABLE::_64B_COMPARE_DISABLE_32B_COMPARE);

        bool commandControlledInhibitContextSwitch = true;
        if (debugManager.flags.OverrideCommandControlledInhibitContextSwitch.get() != -1) {
            commandControlledInhibitContextSwitch = debugManager.flags.OverrideCommandControlledInhibitContextSwitch.get() == 1;
        }
        localCmd.setCommandControlledInhibitContextSwitch(commandControlledInhibitContextSwitch);

        bool semaphoreInterrupt = true;
        if (debugManager.flags.OverrideSemaphoreInterrupt.get() != -1) {
            semaphoreInterrupt = debugManager.flags.OverrideSemaphoreInterrupt.get() == 1;
        }
        localCmd.setSemaphoreInterrupt(semaphoreInterrupt);

        if (switchOnUnsuccessful) {
            localCmd.setQueueSwitchMode(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL);
        }

        if (indirect && useQwordData) {
            localCmd.setSemaphoreDataDword(0);
        }

        *cmd = localCmd;
    } else {
        using MI_SEMAPHORE_WAIT_LEGACY = typename Family::MI_SEMAPHORE_WAIT_LEGACY;
        using COMPARE_OPERATION_LEGACY = typename MI_SEMAPHORE_WAIT_LEGACY::COMPARE_OPERATION;
        using _64B_COMPARE_ENABLE_WITH_GPR = typename MI_SEMAPHORE_WAIT_LEGACY::_64B_COMPARE_ENABLE_WITH_GPR;

        MI_SEMAPHORE_WAIT_LEGACY localCmd = Family::cmdInitMiSemaphoreWaitLegacy;
        localCmd.setCompareOperation(static_cast<COMPARE_OPERATION_LEGACY>(compareMode));
        localCmd.setSemaphoreDataDword(static_cast<uint32_t>(compareData));
        localCmd.setSemaphoreGraphicsAddress(compareAddress);
        localCmd.setWaitMode(waitMode ? MI_SEMAPHORE_WAIT_LEGACY::WAIT_MODE::WAIT_MODE_POLLING_MODE : MI_SEMAPHORE_WAIT_LEGACY::WAIT_MODE::WAIT_MODE_SIGNAL_MODE);
        localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT_LEGACY::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT_LEGACY::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);
        localCmd.setIndirectSemaphoreDataDword(indirect);

        if (indirect && useQwordData) {
            localCmd.set64BCompareEnableWithGpr(_64B_COMPARE_ENABLE_WITH_GPR::_64B_COMPARE_ENABLE_WITH_GPR_64B_GPR_COMPARE);
            localCmd.setIndirectSemaphoreDataDword(false);
            localCmd.setSemaphoreDataDword(0);
        }

        if (switchOnUnsuccessful) {
            localCmd.setQueueSwitchMode(MI_SEMAPHORE_WAIT_LEGACY::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL);
        }

        static_assert(sizeof(MI_SEMAPHORE_WAIT) == sizeof(typename Family::MI_SEMAPHORE_WAIT_LEGACY));
        static_assert(std::is_trivially_copyable_v<MI_SEMAPHORE_WAIT_LEGACY>);
        memcpy(cmd, &localCmd, sizeof(MI_SEMAPHORE_WAIT));
    }
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setCommandLevelInterrupt(CommandType &cmd, bool interrupt) {}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void NEO::EncodePostSync<Family>::setCommandLevelInterrupt<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, bool interrupt);
} // namespace NEO
