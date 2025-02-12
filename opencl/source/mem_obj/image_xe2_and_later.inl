/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"

namespace NEO {
template <>
void ImageHw<Family>::adjustDepthLimitations(RENDER_SURFACE_STATE *surfaceState, uint32_t minArrayElement, uint32_t renderTargetViewExtent, uint32_t depth, uint32_t mipCount, bool is3DUavOrRtv) {
    if (is3DUavOrRtv) {
        auto newDepth = std::min(depth, (renderTargetViewExtent + minArrayElement) << mipCount);
        surfaceState->setDepth(newDepth);
    }
}

template <>
void ImageHw<Family>::setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    if (getMcsAllocation() || getIsUnifiedMcsSurface()) {
        auto mcsGmm = getMcsAllocation() ? getMcsAllocation()->getDefaultGmm() : getMultiGraphicsAllocation().getDefaultGraphicsAllocation()->getDefaultGmm();
        auto *releaseHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
        DEBUG_BREAK_IF(releaseHelper == nullptr);
        EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(surfaceState, releaseHelper);
        surfaceState->setAuxiliarySurfacePitch(mcsGmm->getUnifiedAuxPitchTiles());
        surfaceState->setAuxiliarySurfaceQPitch(mcsGmm->getAuxQPitch());
        EncodeSurfaceState<Family>::setClearColorParams(surfaceState, mcsGmm);
        ImageSurfaceStateHelper<Family>::setUnifiedAuxBaseAddress(surfaceState, mcsGmm);
    } else if (isDepthFormat(imageFormat) && surfaceState->getSurfaceFormat() != SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS) {
        surfaceState->setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    }
}

} // namespace NEO