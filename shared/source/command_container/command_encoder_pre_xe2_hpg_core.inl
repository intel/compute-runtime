/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultIOHAlignment() {
    return 1;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::getThreadCountPerSubslice(const HardwareInfo &hwInfo) {
    return hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignPreferredSlmSize(uint32_t slmSize) {
    return EncodeDispatchKernel<Family>::alignSlmSize(slmSize);
}

template <typename Family>
void EncodeSurfaceState<Family>::disableCompressionFlags(R_SURFACE_STATE *surfaceState) {
    surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    surfaceState->setMemoryCompressionEnable(false);
}

} // namespace NEO
