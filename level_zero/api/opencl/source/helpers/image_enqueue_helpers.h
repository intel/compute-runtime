/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"

#include "CL/cl.h"

#include <cstddef>
#include <cstdint>

// The shared built-in image<->buffer copy kernels address the host buffer using the row-pitch
// kernel argument for the work-item Y dimension. For a 1D image array the Y dimension is the
// array-layer index, so the distance between consecutive array elements (the slice pitch) must be
// supplied as the row pitch passed to the L0 copy entrypoints. This matters for mip-mapped 1D
// arrays, where the per-element slice pitch (spanning the whole mip chain) is larger than the row
// pitch. Mirrors the legacy OpenCL path (see enqueueReadImage / enqueueWriteImage). For every other
// image type, and when no distinct slice pitch is supplied, the row pitch is used unchanged.
inline uint32_t getL0ImageRowPitch(cl_mem_object_type imageType, size_t rowPitch, size_t slicePitch) {
    if (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY && slicePitch > rowPitch) {
        return static_cast<uint32_t>(slicePitch);
    }
    return static_cast<uint32_t>(rowPitch);
}

inline void resolveHostPitchesForCustomPitchImage(bool imageHasCustomPitch, size_t elementSize,
                                                  const size_t *region, size_t &rowPitch, size_t &slicePitch) {
    if (!imageHasCustomPitch) {
        return;
    }
    if (rowPitch == 0) {
        rowPitch = region[0] * elementSize;
    }
    if (slicePitch == 0) {
        slicePitch = region[1] * rowPitch;
    }
}

namespace NEO {
namespace LEO {

inline uint32_t getOclMipLevel(NEO::LEO::Image *pImage, const size_t *origin) {
    if (pImage == nullptr || origin == nullptr) {
        return 0u;
    }
    auto numMipLevels = pImage->getL0Object()->getImageInfo().imgDesc.numMipLevels;
    if (numMipLevels <= 1u) {
        return 0u;
    }
    switch (pImage->getClObjectType()) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return static_cast<uint32_t>(origin[1]);
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return static_cast<uint32_t>(origin[2]);
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return static_cast<uint32_t>(origin[3]);
    default:
        return 0u;
    }
}

inline L0::ze_image_region_mip_level_exp_desc_t createZeImageRegionWithMipLevel(
    NEO::LEO::Image *pImage,
    const size_t *origin,
    const size_t *region) {

    L0::ze_image_region_mip_level_exp_desc_t desc{};
    uint32_t mipLevel = getOclMipLevel(pImage, origin);

    switch (pImage->getClObjectType()) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = 0u;
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = 1u;
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = static_cast<uint32_t>(origin[2]);
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = static_cast<uint32_t>(region[2]);
        break;
    default:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = static_cast<uint32_t>(origin[2]);
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = static_cast<uint32_t>(region[2]);
        break;
    }
    desc.mipLevel = mipLevel;
    return desc;
}

} // namespace LEO
} // namespace NEO
