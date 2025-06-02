/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_from_xe3_and_later.inl"
#include "shared/source/command_container/command_encoder_from_xe_hpg_core_to_xe3_core.inl"
#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe2_hpg_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpc_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

using Family = NEO::Xe3CoreFamily;
namespace NEO {

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMask1();
    auto maskBits2 = stateComputeMode.getMask2();

    if (properties.isPipelinedEuThreadArbitrationEnabled()) {
        stateComputeMode.setEnablePipelinedEuThreadArbitration(true);
        maskBits |= Family::stateComputeModePipelinedEuThreadArbitrationMask;
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

    stateComputeMode.setMask1(maskBits);
    stateComputeMode.setMask2(maskBits2);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS);
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    if (walkerArgs.hasSample) {
        walkerCmd.setDispatchWalkOrder(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK);
        walkerCmd.setThreadGroupBatchSize(DefaultWalkerType::THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4);
    }
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
    using INTERFACE_DESCRIPTOR_DATA = typename Family::INTERFACE_DESCRIPTOR_DATA;
    if constexpr (std::is_same_v<InterfaceDescriptorType, INTERFACE_DESCRIPTOR_DATA>) {

        auto pipelinedThreadArbitrationPolicy = kernelDesc.kernelAttributes.threadArbitrationPolicy;

        if (pipelinedThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) {
            pipelinedThreadArbitrationPolicy = static_cast<ThreadArbitrationPolicy>(defaultPipelinedThreadArbitrationPolicy);
        }

        switch (pipelinedThreadArbitrationPolicy) {
        case ThreadArbitrationPolicy::RoundRobin:
            pInterfaceDescriptor->setEuThreadSchedulingModeOverride(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN);
            break;
        case ThreadArbitrationPolicy::AgeBased:
            pInterfaceDescriptor->setEuThreadSchedulingModeOverride(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST);
            break;
        case ThreadArbitrationPolicy::RoundRobinAfterDependency:
            pInterfaceDescriptor->setEuThreadSchedulingModeOverride(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN);
            break;
        default:
            pInterfaceDescriptor->setEuThreadSchedulingModeOverride(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN);
        }
    }
}

template <>
bool EncodeEnableRayTracing<Family>::is48bResourceNeededForRayTracing() {
    if (debugManager.flags.Enable64bAddressingForRayTracing.get() != -1) {
        return !debugManager.flags.Enable64bAddressingForRayTracing.get();
    }

    return false;
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType);
template struct EncodeDispatchKernelWithHeap<Family>;
template void NEO::EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &, unsigned int, unsigned int);

} // namespace NEO
