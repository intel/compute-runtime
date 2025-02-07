/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/hw_walk_order.h"

namespace NEO {
template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultIOHAlignment() {
    size_t alignment = Family::cacheLineSize;
    if (NEO::debugManager.flags.ForceIOHAlignment.get() != -1) {
        alignment = static_cast<size_t>(debugManager.flags.ForceIOHAlignment.get());
    }
    return alignment;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::getThreadCountPerSubslice(const HardwareInfo &hwInfo) {
    return hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignPreferredSlmSize(uint32_t slmSize) {
    return slmSize;
}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {
    bool computeDispatchAllWalkerEnable = walkerArgs.kernelExecutionType == KernelExecutionType::concurrent || (rootDeviceEnvironment.getProductHelper().adjustDispatchAllRequired(*rootDeviceEnvironment.getHardwareInfo()) &&
                                                                                                                idd &&
                                                                                                                idd->getThreadGroupDispatchSize() == InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1 &&
                                                                                                                walkerCmd.getThreadGroupIdXDimension() * walkerCmd.getThreadGroupIdYDimension() * walkerCmd.getThreadGroupIdZDimension() * idd->getNumberOfThreadsInGpgpuThreadGroup() < walkerArgs.maxFrontEndThreads);
    int32_t overrideComputeDispatchAllWalkerEnable = debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.get();
    if (overrideComputeDispatchAllWalkerEnable != -1) {
        computeDispatchAllWalkerEnable = !!overrideComputeDispatchAllWalkerEnable;
    }
    walkerCmd.setComputeDispatchAllWalkerEnable(computeDispatchAllWalkerEnable);
}

template <typename Family>
void EncodeSurfaceState<Family>::disableCompressionFlags(R_SURFACE_STATE *surfaceState) {
}

template <typename Family>
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

template <typename Family>
void EncodeSurfaceState<Family>::setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <typename Family>
inline void EncodeSurfaceState<Family>::setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <typename Family>
void EncodeSurfaceState<Family>::setImageAuxParamsForCCS(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <typename Family>
void EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(R_SURFACE_STATE *surfaceState) {
}

template <typename Family>
bool EncodeSurfaceState<Family>::isAuxModeEnabled(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    return gmm && gmm->isCompressionEnabled();
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::linearWalk) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK);
    } else if (HwWalkOrderHelper::compatibleDimensionOrders[requiredWorkGroupOrder] == HwWalkOrderHelper::yOrderWalk) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK);
    }
}

template <typename Family>
void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
}

template <typename Family>
void EncodeWA<Family>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
}

template <typename Family>
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

template <typename Family>
inline void EncodeMiFlushDW<Family>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {
    miFlushDwCmd->setFlushLlc(1);

    if (productHelper.isDcFlushAllowed()) {
        miFlushDwCmd->setFlushCcs(1);
    }
}

template <typename Family>
bool EncodeSurfaceState<Family>::shouldProgramAuxForMcs(bool isAuxCapable, bool hasMcsSurface) {
    return hasMcsSurface;
}

} // namespace NEO
