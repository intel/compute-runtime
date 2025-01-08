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

        uint32_t prefetchSize = sizeInBytesToPrefetch / MemoryConstants::cacheLineSize;

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
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
    UNRECOVERABLE_IF(releaseHelper == nullptr);
    if (releaseHelper->isDisablingMsaaRequired()) {
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    if (walkerArgs.kernelDescriptor.kernelAttributes.flags.hasSample) {
        walkerCmd.setDispatchWalkOrder(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK);
        walkerCmd.setThreadGroupBatchSize(DefaultWalkerType::THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4);
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
} // namespace NEO
