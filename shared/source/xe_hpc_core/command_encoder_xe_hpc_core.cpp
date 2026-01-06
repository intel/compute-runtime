/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_from_gen12lp_to_xe2_hpg.inl"
#include "shared/source/command_container/command_encoder_from_xe_hpg_core_to_xe2_hpg.inl"
#include "shared/source/command_container/command_encoder_from_xe_hpg_core_to_xe3_core.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpc_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_xe_hpc.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

using Family = NEO::XeHpcCoreFamily;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"

namespace NEO {

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

    stateComputeMode.setMaskBits(maskBits);

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    productHelper.updateScmCommand(&stateComputeMode, properties);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
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
template <typename InterfaceDescriptorType>
void EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch(InterfaceDescriptorType &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount) {
    auto enablePrefetch = EncodeSurfaceState<Family>::doBindingTablePrefetch();

    if (enablePrefetch) {
        interfaceDescriptor.setBindingTableEntryCount(std::min(bindingTableEntryCount, 31u));
    } else {
        interfaceDescriptor.setBindingTableEntryCount(0u);
    }
}

template <>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {
    int32_t overrideDispatchAllWalkerEnableInComputeWalker = debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get();
    if (overrideDispatchAllWalkerEnableInComputeWalker != -1) {
        walkerCmd.setComputeDispatchAllWalkerEnable(overrideDispatchAllWalkerEnableInComputeWalker);
    }
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType);
template struct EncodeDispatchKernelWithHeap<Family>;
template void NEO::EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &, unsigned int, unsigned int);

} // namespace NEO
