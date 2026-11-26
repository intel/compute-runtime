/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NEO {
struct ImageInfo;
class Gmm;
class GraphicsAllocation;
struct RootDeviceEnvironment;

struct ReducedMultisampleSurfaceLayout {
    uint64_t surfaceBaseAddress = 0;
    uint64_t mcsOffset = 0;
};

struct ImageHelper {
    static bool areImagesCompatibleWithPackedFormat(const RootDeviceEnvironment &rootDeviceEnvironment, const ImageInfo &srcImageInfo, const ImageInfo &dstImageInfo, const GraphicsAllocation *srcAllocation, const GraphicsAllocation *dstAllocation, size_t copyWidth);
    static uint64_t getMultisampleControlSurfaceAddress(const GraphicsAllocation &imageAllocation, const GraphicsAllocation *mcsAllocation, bool isUnifiedMcsSurface);
    static bool getReducedMultisampleSurfaceLayout(const Gmm *gmm, uint64_t surfaceAddress, uint64_t mcsSurfaceAddress, ReducedMultisampleSurfaceLayout &layout);
    static bool isMultisampleControlSurfaceLayoutSupported(const GraphicsAllocation &imageAllocation, const GraphicsAllocation *mcsAllocation, bool isUnifiedMcsSurface, uint64_t surfaceOffset);

    static uint32_t getVolumeDepthForMipView(uint32_t maxDepth, uint32_t viewDepth, uint32_t mipLevel) {
        return static_cast<uint32_t>(std::min<uint64_t>(maxDepth, static_cast<uint64_t>(viewDepth) << mipLevel));
    }
};

} // namespace NEO
