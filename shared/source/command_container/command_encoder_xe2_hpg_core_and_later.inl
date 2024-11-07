/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

} // namespace NEO
