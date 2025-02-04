/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

class Gmm;
struct SurfaceOffsets;
class GmmHelper;
struct ImageInfo;

template <typename GfxFamily>
class ImageSurfaceStateHelper {
  public:
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    static void setImageSurfaceState(RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo, Gmm *gmm, GmmHelper &gmmHelper, uint32_t cubeFaceIndex, uint64_t gpuAddress, const SurfaceOffsets &surfaceOffsets, bool isNV12Format, uint32_t &minimumArrayElement, uint32_t &renderTargetViewExtent);
    static void setImageSurfaceStateDimensions(RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo, uint32_t cubeFaceIndex, SURFACE_TYPE surfaceType, uint32_t &depth);
    static void setWidthForMediaBlockSurfaceState(RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo);
    static void setUnifiedAuxBaseAddress(RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm);
    static void setMipTailStartLOD(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm);
    static bool imageAsArrayWithArraySizeOf1NotPreferred();
};
} // namespace NEO
