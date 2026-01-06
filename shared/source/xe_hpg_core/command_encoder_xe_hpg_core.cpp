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
#include "shared/source/command_container/command_encoder_gen12lp_and_xe_hpg.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/command_encoder_xe_hpg_core_and_xe_hpc.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/lookup_array.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

using Family = NEO::XeHpgCoreFamily;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"

namespace NEO {

template <>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeThreadGroupDispatch(InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo,
                                                             const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t requiredThreadGroupDispatchSize,
                                                             const uint32_t grfCount, const uint32_t threadsPerThreadGroup, WalkerType &walkerCmd) {
    const auto &productHelper = device.getProductHelper();
    if (requiredThreadGroupDispatchSize != 0) {
        interfaceDescriptor.setThreadGroupDispatchSize(static_cast<typename InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE>(requiredThreadGroupDispatchSize));
    } else if (productHelper.isDisableOverdispatchAvailable(hwInfo)) {
        if (threadsPerThreadGroup == 1) {
            interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(2u));
        } else {
            interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(3u));
        }
    }

    if (debugManager.flags.ForceThreadGroupDispatchSize.get() != -1) {
        interfaceDescriptor.setThreadGroupDispatchSize(
            static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(debugManager.flags.ForceThreadGroupDispatchSize.get()));
    }
}

template <>
template <>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const KernelDescriptor &kernelDescriptor, const HardwareInfo &hwInfo) {
    using BARRIERS = INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS;
    static const LookupArray<uint32_t, BARRIERS, 2> barrierLookupArray({{{0, BARRIERS::NUMBER_OF_BARRIERS_NONE},
                                                                         {1, BARRIERS::NUMBER_OF_BARRIERS_B1}}});
    BARRIERS numBarriers = barrierLookupArray.lookUp(kernelDescriptor.kernelAttributes.barrierCount);
    interfaceDescriptor.setNumberOfBarriers(numBarriers);
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields(WalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {}

template <>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    UNRECOVERABLE_IF(!releaseHelper);
    bool ignoreIsDirty = releaseHelper->isProgramAllStateComputeCommandFieldsWARequired();

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

    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    productHelper.setForceNonCoherent(&stateComputeMode, properties);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <>
void EncodeSurfaceState<Family>::appendParamsForImageFromBuffer(R_SURFACE_STATE *surfaceState) {
    const auto ccsMode = R_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E;
    if (ccsMode == surfaceState->getAuxiliarySurfaceMode() && R_SURFACE_STATE::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D == surfaceState->getSurfaceType()) {
        if (debugManager.flags.DecompressInL3ForImage2dFromBuffer.get() != 0) {
            surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
            surfaceState->setDecompressInL3(R_SURFACE_STATE::DECOMPRESS_IN_L3_ENABLE);
            surfaceState->setMemoryCompressionEnable(1);
            surfaceState->setMemoryCompressionType(R_SURFACE_STATE::MEMORY_COMPRESSION_TYPE::MEMORY_COMPRESSION_TYPE_3D_COMPRESSION);
        }
    }
}

template <>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
    if (productHelper.isAdjustWalkOrderAvailable(rootDeviceEnvironment.getReleaseHelper())) {
        if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::linearWalk) {
            walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK);
        } else if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::yOrderWalk) {
            walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK);
        }
    }
}

template <>
uint32_t EncodeDispatchKernel<Family>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 1024u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    UNRECOVERABLE_IF(slmSize > 64u * MemoryConstants::kiloByte);
    return slmSize;
}

template <>
uint32_t EncodeDispatchKernel<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize, ReleaseHelper *releaseHelper, bool isHeapless) {
    auto slmValue = std::max(slmSize, 1024u);
    slmValue = Math::nextPowerOfTwo(slmValue);
    slmValue = Math::getMinLsbSet(slmValue);
    slmValue = slmValue - 9;
    DEBUG_BREAK_IF(slmValue > 7);
    slmValue *= !!slmSize;
    return slmValue;
}

template <>
void adjustL3ControlField<Family>(void *l3ControlBuffer) {
    using L3_CONTROL = typename Family::L3_CONTROL;
    auto l3Control = reinterpret_cast<L3_CONTROL *>(l3ControlBuffer);
    l3Control->setUnTypedDataPortCacheFlush(true);
}

template <>
void EncodeMiFlushDW<Family>::appendWa(LinearStream &commandStream, MiFlushArgs &args) {
    BlitCommandsHelper<Family>::dispatchDummyBlit(commandStream, args.waArgs);
    auto miFlushDwCmd = commandStream.getSpaceForCmd<MI_FLUSH_DW>();
    *miFlushDwCmd = Family::cmdInitMiFlushDw;
}

template <>
size_t EncodeMiFlushDW<Family>::getWaSize(const EncodeDummyBlitWaArgs &waArgs) {
    return sizeof(typename Family::MI_FLUSH_DW) + BlitCommandsHelper<Family>::getDummyBlitSize(waArgs);
}

template <typename Family>
void EncodeMiFlushDW<Family>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {
    miFlushDwCmd->setFlushCcs(1);
    miFlushDwCmd->setFlushLlc(1);
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType);
template void flushGpuCache<Family>(LinearStream *commandStream, std::span<const L3Range> ranges, uint64_t postSyncAddress, const HardwareInfo &hwInfo);
template struct EncodeDispatchKernelWithHeap<Family>;
template void NEO::EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &, unsigned int, unsigned int);

} // namespace NEO
