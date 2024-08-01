/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

using Family = NEO::XeHpcCoreFamily;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpc_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_xehp_and_later.inl"

namespace NEO {

template <>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(writeAddress);
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
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

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    productHelper.updateScmCommand(&stateComputeMode, properties);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeMemoryPrefetch<Family>::programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_PREFETCH = typename Family::STATE_PREFETCH;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    bool prefetch = productHelper.allowMemoryPrefetch(hwInfo);

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

        if (debugManager.flags.ForceCsStallForStatePrefetch.get() == 1) {
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
size_t EncodeMemoryPrefetch<Family>::getSizeForMemoryPrefetch(size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.EnableMemoryPrefetch.get() == 0) {
        return 0;
    }
    size = alignUp(size, MemoryConstants::pageSize64k);

    size_t count = size / MemoryConstants::pageSize64k;

    return (count * sizeof(typename Family::STATE_PREFETCH));
}

template <>
inline void EncodeMiFlushDW<Family>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {
    miFlushDwCmd->setFlushLlc(1);
}

template <>
template <>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor,
                                                        uint32_t value,
                                                        const HardwareInfo &hwInfo) {
    using BARRIERS = INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS;
    static const LookupArray<uint32_t, BARRIERS, 8> barrierLookupArray({{{0, BARRIERS::NUMBER_OF_BARRIERS_NONE},
                                                                         {1, BARRIERS::NUMBER_OF_BARRIERS_B1},
                                                                         {2, BARRIERS::NUMBER_OF_BARRIERS_B2},
                                                                         {4, BARRIERS::NUMBER_OF_BARRIERS_B4},
                                                                         {8, BARRIERS::NUMBER_OF_BARRIERS_B8},
                                                                         {16, BARRIERS::NUMBER_OF_BARRIERS_B16},
                                                                         {24, BARRIERS::NUMBER_OF_BARRIERS_B24},
                                                                         {32, BARRIERS::NUMBER_OF_BARRIERS_B32}}});
    BARRIERS numBarriers = barrierLookupArray.lookUp(value);
    interfaceDescriptor.setNumberOfBarriers(numBarriers);
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsPostSyncOperationInComputeWalker = productHelper.isGlobalFenceInCommandStreamRequired(hwInfo) &&
                                                                walkerArgs.requiredSystemFence;
    int32_t overrideProgramSystemMemoryFence = debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get();
    if (overrideProgramSystemMemoryFence != -1) {
        programGlobalFenceAsPostSyncOperationInComputeWalker = !!overrideProgramSystemMemoryFence;
    }
    auto &postSyncData = walkerCmd.getPostSync();
    postSyncData.setSystemMemoryFenceRequest(programGlobalFenceAsPostSyncOperationInComputeWalker);

    int32_t forceL3PrefetchForComputeWalker = debugManager.flags.ForceL3PrefetchForComputeWalker.get();
    if (forceL3PrefetchForComputeWalker != -1) {
        walkerCmd.setL3PrefetchDisable(!forceL3PrefetchForComputeWalker);
    }

    int32_t overrideDispatchAllWalkerEnableInComputeWalker = debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get();
    if (overrideDispatchAllWalkerEnableInComputeWalker != -1) {
        walkerCmd.setComputeDispatchAllWalkerEnable(overrideDispatchAllWalkerEnableInComputeWalker);
    }
}

template <>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename InterfaceDescriptorType::PREFERRED_SLM_ALLOCATION_SIZE;
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const uint32_t threadsPerDssCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
    const uint32_t workGroupCountPerDss = static_cast<uint32_t>(Math::divideAndRoundUp(threadsPerDssCount, threadsPerThreadGroup));

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    const uint32_t workgroupSlmSize = gfxCoreHelper.alignSlmSize(slmTotalSize);

    uint32_t slmSize = 0u;

    switch (slmPolicy) {
    case SlmPolicy::slmPolicyLargeData:
        slmSize = workgroupSlmSize;
        break;
    case SlmPolicy::slmPolicyLargeSlm:
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
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K},
    }};

    auto programmableIdPreferredSlmSize = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K;
    for (auto &range : ranges) {
        if (slmSize <= range.upperLimit) {
            programmableIdPreferredSlmSize = range.valueToProgram;
            break;
        }
    }

    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    if ((slmSize == 0) && (productHelper.isAdjustProgrammableIdPreferredSlmSizeRequired(hwInfo))) {
        programmableIdPreferredSlmSize = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K;
    }

    pInterfaceDescriptor->setPreferredSlmAllocationSize(programmableIdPreferredSlmSize);

    if (debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get() != -1) {
        auto toProgram =
            static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get());
        pInterfaceDescriptor->setPreferredSlmAllocationSize(toProgram);
    }
}

template <>
void EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount) {
    auto enablePrefetch = EncodeSurfaceState<Family>::doBindingTablePrefetch();

    if (enablePrefetch) {
        interfaceDescriptor.setBindingTableEntryCount(std::min(bindingTableEntryCount, 31u));
    } else {
        interfaceDescriptor.setBindingTableEntryCount(0u);
    }
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
} // namespace NEO
