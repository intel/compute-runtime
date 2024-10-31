/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_container/command_encoder_xe2_hpg_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

using Family = NEO::Xe2HpgCoreFamily;

#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpc_core_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_xehp_and_later.inl"

namespace NEO {

template <>
void EncodeEnableRayTracing<Family>::append3dStateBtd(void *ptr3dStateBtd) {
    using _3DSTATE_BTD = typename Family::_3DSTATE_BTD;
    using DISPATCH_TIMEOUT_COUNTER = typename Family::_3DSTATE_BTD_BODY::DISPATCH_TIMEOUT_COUNTER;
    using CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS = typename Family::_3DSTATE_BTD_BODY::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS;
    auto cmd = static_cast<_3DSTATE_BTD *>(ptr3dStateBtd);
    if (debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get() != -1) {
        auto value = static_cast<CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS>(debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get());
        DEBUG_BREAK_IF(value > 3);
        cmd->getBtdStateBody().setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(value);
    }
    if (debugManager.flags.ForceDispatchTimeoutCounter.get() != -1) {
        auto value = static_cast<DISPATCH_TIMEOUT_COUNTER>(debugManager.flags.ForceDispatchTimeoutCounter.get());
        DEBUG_BREAK_IF(value > 7);
        cmd->getBtdStateBody().setDispatchTimeoutCounter(value);
    }
}

template <>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(writeAddress);
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMask1();
    auto maskBits2 = stateComputeMode.getMask2();

    if (properties.memoryAllocationForScratchAndMidthreadPreemptionBuffers.isDirty) {
        stateComputeMode.setMemoryAllocationForScratchAndMidthreadPreemptionBuffers(properties.memoryAllocationForScratchAndMidthreadPreemptionBuffers.value);
        maskBits2 |= Family::stateComputeModeMemoryAllocationForScratchAndMidthreadPreemptionBuffersMask;
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

    stateComputeMode.setMask1(maskBits);
    stateComputeMode.setMask2(maskBits2);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeWA<Family>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
}

template <>
void EncodeMemoryPrefetch<Family>::programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_PREFETCH = typename Family::STATE_PREFETCH;
    constexpr uint32_t mocsIndexForL3 = (1 << 1);

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
size_t EncodeMemoryPrefetch<Family>::getSizeForMemoryPrefetch(size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (!productHelper.allowMemoryPrefetch(hwInfo)) {
        return 0;
    }

    size = alignUp(size, MemoryConstants::pageSize64k);

    size_t count = size / MemoryConstants::pageSize64k;

    return (count * sizeof(typename Family::STATE_PREFETCH));
}

template <>
inline void EncodeMiFlushDW<Family>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {
    miFlushDwCmd->setFlushLlc(1);

    if (productHelper.isDcFlushAllowed()) {
        miFlushDwCmd->setFlushCcs(1);
    }
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
void EncodeSurfaceState<Family>::setImageAuxParamsForCCS(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <>
void EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(R_SURFACE_STATE *surfaceState) {
}

template <>
bool EncodeSurfaceState<Family>::isAuxModeEnabled(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    return gmm && gmm->isCompressionEnabled();
}

template <>
void EncodeSurfaceState<Family>::appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation,
                                                              GmmHelper *gmmHelper, bool imageFromBuffer, GMM_YUV_PLANE_ENUM plane) {
    if (!allocation->isCompressionEnabled()) {
        return;
    }

    using COMPRESSION_FORMAT = typename R_SURFACE_STATE::COMPRESSION_FORMAT;

    auto gmmResourceInfo = allocation->getDefaultGmm()->gmmResourceInfo.get();

    auto compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(gmmResourceInfo->getResourceFormat());

    surfaceState->setCompressionFormat(static_cast<COMPRESSION_FORMAT>(compressionFormat));
}

template <>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
    if (releaseHelper && releaseHelper->isAuxSurfaceModeOverrideRequired())
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS);
}

template <>
void EncodeSurfaceState<Family>::setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <>
inline void EncodeSurfaceState<Family>::setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <>
void EncodeSurfaceState<Family>::disableCompressionFlags(R_SURFACE_STATE *surfaceState) {
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    bool computeDispatchAllWalkerEnable = walkerArgs.kernelExecutionType == KernelExecutionType::concurrent;
    int32_t overrideComputeDispatchAllWalkerEnable = debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get();
    if (overrideComputeDispatchAllWalkerEnable != -1) {
        computeDispatchAllWalkerEnable = !!overrideComputeDispatchAllWalkerEnable;
    }
    walkerCmd.setComputeDispatchAllWalkerEnable(computeDispatchAllWalkerEnable);
}

template <>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const RootDeviceEnvironment &rootDeviceEnvironment) {}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::linearWalk) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::LINERAR_WALKER);
    } else if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::yOrderWalk) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::Y_ORDER_WALKER);
    }
}

template <>
void EncodeSurfaceState<Family>::setCoherencyType(Family::RENDER_SURFACE_STATE *surfaceState, Family::RENDER_SURFACE_STATE::COHERENCY_TYPE coherencyType) {
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);

} // namespace NEO
