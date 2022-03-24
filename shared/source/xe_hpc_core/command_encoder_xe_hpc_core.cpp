/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_base.h"

using Family = NEO::XE_HPC_COREFamily;

#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_xehp_and_later.inl"

namespace NEO {

template <>
void EncodeDispatchKernel<Family>::adjustTimestampPacket(WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo) {
    walkerCmd.getPostSync().setDataportSubsliceCacheFlush(true);
}

template <>
void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const HardwareInfo &hwInfo) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
        interfaceDescriptor.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
    }

    if (DebugManager.flags.ForceThreadGroupDispatchSize.get() != -1) {
        interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(
            DebugManager.flags.ForceThreadGroupDispatchSize.get()));
    }
}

template <>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(writeAddress);
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    if (properties.isCoherencyRequired.isDirty) {
        FORCE_NON_COHERENT coherencyValue = !properties.isCoherencyRequired.value ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT
                                                                                  : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED;
        stateComputeMode.setForceNonCoherent(coherencyValue);
        maskBits |= Family::stateComputeModeForceNonCoherentMask;
    }

    if (properties.threadArbitrationPolicy.isDirty) {
        switch (properties.threadArbitrationPolicy.value) {
        case ThreadArbitrationPolicy::RoundRobin:
            stateComputeMode.setEuThreadSchedulingModeOverride(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN);
            break;
        case ThreadArbitrationPolicy::AgeBased:
            stateComputeMode.setEuThreadSchedulingModeOverride(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST);
            break;
        case ThreadArbitrationPolicy::RoundRobinAfterDependency:
            stateComputeMode.setEuThreadSchedulingModeOverride(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN);
            break;
        default:
            stateComputeMode.setEuThreadSchedulingModeOverride(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT);
        }
        maskBits |= Family::stateComputeModeEuThreadSchedulingModeOverrideMask;
    }

    if (properties.largeGrfMode.isDirty) {
        stateComputeMode.setLargeGrfMode(properties.largeGrfMode.value);
        maskBits |= Family::stateComputeModeLargeGrfModeMask;
    }

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeMemoryPrefetch<Family>::programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const HardwareInfo &hwInfo) {
    using STATE_PREFETCH = typename Family::STATE_PREFETCH;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);

    bool isBaseDieA0 = (hwInfo.platform.usRevId & Family::pvcBaseDieRevMask) == Family::pvcBaseDieA0Masked;

    bool prefetch = !isBaseDieA0;
    if (DebugManager.flags.EnableMemoryPrefetch.get() != -1) {
        prefetch = !!DebugManager.flags.EnableMemoryPrefetch.get();
    }

    if (!prefetch) {
        return;
    }

    uint64_t gpuVa = graphicsAllocation.getGpuAddress() + offset;

    while (size > 0) {
        uint32_t sizeInBytesToPrefetch = std::min(alignUp(size, MemoryConstants::cacheLineSize),
                                                  static_cast<uint32_t>(MemoryConstants::pageSize64k));

        // zero based cacheline count (0 == 1 cacheline)
        uint32_t prefetchSize = (sizeInBytesToPrefetch / MemoryConstants::cacheLineSize) - 1;

        auto statePrefetch = commandStream.getSpaceForCmd<STATE_PREFETCH>();
        STATE_PREFETCH cmd = Family::cmdInitStatePrefetch;

        cmd.setAddress(gpuVa);
        cmd.setPrefetchSize(prefetchSize);
        cmd.setMemoryObjectControlState(mocsIndexForL3);
        cmd.setKernelInstructionPrefetch(GraphicsAllocation::isIsaAllocationType(graphicsAllocation.getAllocationType()));

        if (DebugManager.flags.ForceCsStallForStatePrefetch.get() == 1) {
            cmd.setParserStall(true);
        }

        *statePrefetch = cmd;

        if (sizeInBytesToPrefetch > size) {
            break;
        }

        gpuVa += sizeInBytesToPrefetch;
        size -= sizeInBytesToPrefetch;
    }
}

template <>
size_t EncodeMemoryPrefetch<Family>::getSizeForMemoryPrefetch(size_t size) {
    if (DebugManager.flags.EnableMemoryPrefetch.get() == 0) {
        return 0;
    }

    size = alignUp(size, MemoryConstants::pageSize64k);

    size_t count = size / MemoryConstants::pageSize64k;

    return (count * sizeof(typename Family::STATE_PREFETCH));
}

template <>
inline void EncodeMiFlushDW<Family>::appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd, const HardwareInfo &hwInfo) {
    miFlushDwCmd->setFlushLlc(1);
}

template <>
inline void EncodeMiFlushDW<Family>::programMiFlushDwWA(LinearStream &commandStream) {
}

template <>
size_t EncodeMiFlushDW<Family>::getMiFlushDwWaSize() {
    return 0;
}

template <>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor,
                                                        uint32_t value,
                                                        const HardwareInfo &hwInfo) {
    interfaceDescriptor.setNumberOfBarriers(static_cast<INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS>(value));
}

template <>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd, KernelExecutionType kernelExecutionType) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto programGlobalFenceAsPostSyncOperationInComputeWalker = hwInfoConfig.isGlobalFenceInCommandStreamRequired(hwInfo);

    if (DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() != -1) {
        programGlobalFenceAsPostSyncOperationInComputeWalker = !!DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get();
    }
    if (programGlobalFenceAsPostSyncOperationInComputeWalker) {
        auto &postSyncData = walkerCmd.getPostSync();
        postSyncData.setSystemMemoryFenceRequest(true);
    }

    if (DebugManager.flags.ForceL3PrefetchForComputeWalker.get() != -1) {
        walkerCmd.setL3PrefetchDisable(!DebugManager.flags.ForceL3PrefetchForComputeWalker.get());
    }

    auto programComputeDispatchAllWalkerEnableInComputeWalker = hwInfoConfig.isComputeDispatchAllWalkerEnableInComputeWalkerRequired(hwInfo);
    if (programComputeDispatchAllWalkerEnableInComputeWalker) {
        if (kernelExecutionType == KernelExecutionType::Concurrent) {
            walkerCmd.setComputeDispatchAllWalkerEnable(true);
        }
    }
    if (DebugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get() != -1) {
        walkerCmd.setComputeDispatchAllWalkerEnable(DebugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get());
    }
}

template <>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const uint32_t threadsPerDssCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
    const uint32_t workGroupCountPerDss = static_cast<uint32_t>(Math::divideAndRoundUp(threadsPerDssCount, threadsPerThreadGroup));
    const uint32_t workgroupSlmSize = HwHelperHw<Family>::get().alignSlmSize(slmTotalSize);

    uint32_t slmSize = 0u;

    switch (slmPolicy) {
    case SlmPolicy::SlmPolicyLargeData:
        slmSize = workgroupSlmSize;
        break;
    case SlmPolicy::SlmPolicyLargeSlm:
    default:
        slmSize = workgroupSlmSize * workGroupCountPerDss;
        break;
    }

    struct SizeToPreferredSlmValue {
        uint32_t upperLimit;
        PREFERRED_SLM_ALLOCATION_SIZE valueToProgram;
    };
    const std::array<SizeToPreferredSlmValue, 6> ranges = {{
        // upper limit, retVal
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K},
        {16 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        {64 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
        {96 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K},
    }};

    auto programmableIdPreferredSlmSize = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K;
    for (auto &range : ranges) {
        if (slmSize <= range.upperLimit) {
            programmableIdPreferredSlmSize = range.valueToProgram;
            break;
        }
    }

    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    if ((slmSize == 0) && (hwInfoConfig.isAdjustProgrammableIdPreferredSlmSizeRequired(hwInfo))) {
        programmableIdPreferredSlmSize = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K;
    }

    pInterfaceDescriptor->setPreferredSlmAllocationSize(programmableIdPreferredSlmSize);

    if (DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.get() != -1) {
        auto toProgram =
            static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.get());
        pInterfaceDescriptor->setPreferredSlmAllocationSize(toProgram);
    }
}

template <>
void EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount) {
    auto enablePrefetch = EncodeSurfaceState<Family>::doBindingTablePrefetch();
    if (DebugManager.flags.ForceBtpPrefetchMode.get() != -1) {
        enablePrefetch = static_cast<bool>(DebugManager.flags.ForceBtpPrefetchMode.get());
    }

    if (enablePrefetch) {
        interfaceDescriptor.setBindingTableEntryCount(std::min(bindingTableEntryCount, 31u));
    } else {
        interfaceDescriptor.setBindingTableEntryCount(0u);
    }
}

template struct EncodeDispatchKernel<Family>;
template struct EncodeStates<Family>;
template struct EncodeMath<Family>;
template struct EncodeMathMMIO<Family>;
template struct EncodeIndirectParams<Family>;
template struct EncodeSetMMIO<Family>;
template struct EncodeMediaInterfaceDescriptorLoad<Family>;
template struct EncodeStateBaseAddress<Family>;
template struct EncodeStoreMMIO<Family>;
template struct EncodeSurfaceState<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeAtomic<Family>;
template struct EncodeSempahore<Family>;
template struct EncodeBatchBufferStartOrEnd<Family>;
template struct EncodeMiFlushDW<Family>;
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeWA<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
} // namespace NEO
