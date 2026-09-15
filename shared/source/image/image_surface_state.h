/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

namespace NEO {

class Gmm;
struct SurfaceOffsets;
class GmmHelper;
struct ImageInfo;

struct ImageSurfaceStateInputs {
    const ImageInfo *imageInfo = nullptr;
    Gmm *gmm = nullptr;
    GmmHelper *gmmHelper = nullptr;
    const SurfaceOffsets *surfaceOffsets = nullptr;
    uint64_t gpuAddress = 0;
    uint32_t cubeFaceIndex = 7u; // __GMM_NO_CUBE_MAP sentinel; every caller sets this explicitly.

    // Caller-resolved surface-view parameters (raw HW-field values).
    uint32_t shaderChannelSelectRed = 0;
    uint32_t shaderChannelSelectGreen = 0;
    uint32_t shaderChannelSelectBlue = 0;
    uint32_t shaderChannelSelectAlpha = 0;
    uint32_t numberOfMultisamples = 0;
    uint32_t surfaceMinLOD = 0;
    uint32_t mipCountLOD = 0;
    uint32_t compressionFormat = 0;

    uint32_t packedSurfaceFormat = 0;
    uint32_t packedWidth = 0;
    uint32_t packedTileBpp = 0;

    bool isNV12Format = false;
    bool isDepthStencilResource = false;
    bool useChannelSelects = false;
    bool packedFormatOverride = false;
    bool multisampleControlSurfacePresent = false;
    // Protected-content images carry a decryption bit in the caller-built state that cannot be
    // re-derived from the resource; the caller forwards it here so the encoder can preserve it.
    bool encryptedData = false;
};

template <typename SurfaceState, typename = void>
struct SurfaceStateHasCompressionFormat : std::false_type {};
template <typename SurfaceState>
struct SurfaceStateHasCompressionFormat<SurfaceState, std::void_t<decltype(std::declval<const SurfaceState>().getCompressionFormat())>> : std::true_type {};

template <typename SurfaceState>
uint32_t getSurfaceStateCompressionFormatIfSupported(const SurfaceState &surfaceState) {
    if constexpr (SurfaceStateHasCompressionFormat<SurfaceState>::value) {
        return static_cast<uint32_t>(surfaceState.getCompressionFormat());
    } else {
        return 0u;
    }
}

template <typename SurfaceState, typename = void>
struct SurfaceStateHasOverrideTileBpp : std::false_type {};
template <typename SurfaceState>
struct SurfaceStateHasOverrideTileBpp<SurfaceState, std::void_t<decltype(std::declval<const SurfaceState>().getOverrideTileBPP())>> : std::true_type {};

template <typename SurfaceState>
uint32_t getSurfaceStateOverrideTileBppIfSupported(const SurfaceState &surfaceState) {
    if constexpr (SurfaceStateHasOverrideTileBpp<SurfaceState>::value) {
        return static_cast<uint32_t>(surfaceState.getOverrideTileBPP());
    } else {
        return 0u;
    }
}

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
