/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

using Family = NEO::XeHpgCoreFamily;

#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_xehp_and_later.inl"

namespace NEO {

template <>
void EncodeDispatchKernel<Family>::adjustTimestampPacket(WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo) {
    auto &postSyncData = walkerCmd.getPostSync();

    postSyncData.setDataportSubsliceCacheFlush(true);
}

template <>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const uint32_t threadsPerDssCount = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
    const uint32_t workGroupCountPerDss = threadsPerDssCount / threadsPerThreadGroup;
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    const uint32_t workgroupSlmSize = gfxCoreHelper.alignSlmSize(slmTotalSize);

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

    if (ProductHelper::get(hwInfo.platform.eProductFamily)->isAllocationSizeAdjustmentRequired(hwInfo)) {
        pInterfaceDescriptor->setPreferredSlmAllocationSize(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K);
    } else {
        pInterfaceDescriptor->setPreferredSlmAllocationSize(programmableIdPreferredSlmSize);
    }

    if (DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.get() != -1) {
        auto toProgram =
            static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.get());
        pInterfaceDescriptor->setPreferredSlmAllocationSize(toProgram);
    }
}

template <>
void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t numGrf) {
    const auto &productHelper = device.getProductHelper();
    if (productHelper.isDisableOverdispatchAvailable(hwInfo)) {
        if (interfaceDescriptor.getNumberOfThreadsInGpgpuThreadGroup() == 1) {
            interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(2u));
        } else {
            interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(3u));
        }
    }

    if (DebugManager.flags.ForceThreadGroupDispatchSize.get() != -1) {
        interfaceDescriptor.setThreadGroupDispatchSize(
            static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(DebugManager.flags.ForceThreadGroupDispatchSize.get()));
    }
}

template <>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo) {
    using BARRIERS = INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS;
    static const LookupArray<uint32_t, BARRIERS, 2> barrierLookupArray({{{0, BARRIERS::NUMBER_OF_BARRIERS_NONE},
                                                                         {1, BARRIERS::NUMBER_OF_BARRIERS_B1}}});
    BARRIERS numBarriers = barrierLookupArray.lookUp(value);
    interfaceDescriptor.setNumberOfBarriers(numBarriers);
}

template <>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    bool l3PrefetchDisable = ProductHelper::get(hwInfo.platform.eProductFamily)->isPrefetchDisablingRequired(hwInfo);
    int32_t overrideL3PrefetchDisable = DebugManager.flags.ForceL3PrefetchForComputeWalker.get();
    if (overrideL3PrefetchDisable != -1) {
        l3PrefetchDisable = !overrideL3PrefetchDisable;
    }
    walkerCmd.setL3PrefetchDisable(l3PrefetchDisable);
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment, LogicalStateHelper *logicalStateHelper) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    bool ignoreIsDirty = productHelper.programAllStateComputeCommandFields();

    if (properties.zPassAsyncComputeThreadLimit.isDirty ||
        (ignoreIsDirty && (properties.zPassAsyncComputeThreadLimit.value != -1))) {
        auto limitValue = static_cast<Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT>(properties.zPassAsyncComputeThreadLimit.value);
        stateComputeMode.setZPassAsyncComputeThreadLimit(limitValue);
        maskBits |= Family::stateComputeModeZPassAsyncComputeThreadLimitMask;
    }

    if (properties.pixelAsyncComputeThreadLimit.isDirty ||
        (ignoreIsDirty && (properties.pixelAsyncComputeThreadLimit.value != -1))) {
        auto limitValue = static_cast<PIXEL_ASYNC_COMPUTE_THREAD_LIMIT>(properties.pixelAsyncComputeThreadLimit.value);
        stateComputeMode.setPixelAsyncComputeThreadLimit(limitValue);
        maskBits |= Family::stateComputeModePixelAsyncComputeThreadLimitMask;
    }

    if (properties.largeGrfMode.isDirty || ignoreIsDirty) {
        stateComputeMode.setLargeGrfMode(properties.largeGrfMode.value == 1);
        maskBits |= Family::stateComputeModeLargeGrfModeMask;
    }

    stateComputeMode.setMaskBits(maskBits);

    productHelper.setForceNonCoherent(&stateComputeMode, properties);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeSurfaceState<Family>::appendParamsForImageFromBuffer(R_SURFACE_STATE *surfaceState) {
    const auto ccsMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E;
    if (ccsMode == surfaceState->getAuxiliarySurfaceMode() && R_SURFACE_STATE::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D == surfaceState->getSurfaceType()) {
        if (DebugManager.flags.DecompressInL3ForImage2dFromBuffer.get() != 0) {
            surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
            surfaceState->setDecompressInL3(R_SURFACE_STATE::DECOMPRESS_IN_L3_ENABLE);
            surfaceState->setMemoryCompressionEnable(1);
            surfaceState->setMemoryCompressionType(R_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_3D_COMPRESSION);
        }
    }
}

template <>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WALKER_TYPE &walkerCmd, uint32_t requiredWorkGroupOrder, const HardwareInfo &hwInfo) {
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    if (productHelper.isAdjustWalkOrderAvailable(hwInfo)) {
        if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::linearWalk) {
            walkerCmd.setDispatchWalkOrder(WALKER_TYPE::DISPATCH_WALK_ORDER::LINERAR_WALKER);
        } else if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::yOrderWalk) {
            walkerCmd.setDispatchWalkOrder(WALKER_TYPE::DISPATCH_WALK_ORDER::Y_ORDER_WALKER);
        }
    }
}

template <>
void adjustL3ControlField<Family>(void *l3ControlBuffer) {
    using L3_CONTROL = typename Family::L3_CONTROL;
    auto l3Control = reinterpret_cast<L3_CONTROL *>(l3ControlBuffer);
    l3Control->setUnTypedDataPortCacheFlush(true);
}

template void flushGpuCache<Family>(LinearStream *commandStream, const Range<L3Range> &ranges, uint64_t postSyncAddress, const HardwareInfo &hwInfo);

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
template struct EncodeMiPredicate<Family>;
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeWA<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
template struct EncodeMemoryFence<Family>;
template struct EncodeKernelArgsBuffer<Family>;
} // namespace NEO
