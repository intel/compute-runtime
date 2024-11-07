/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"

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
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    bool computeDispatchAllWalkerEnable = walkerArgs.kernelExecutionType == KernelExecutionType::concurrent;
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

} // namespace NEO
