/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/surface_format_info.h"

namespace NEO {
enum class ImagePlane;
class GraphicsAllocation;
struct ImageInfo;

struct GmmTypesConverter {
    static void queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc);
    static GmmCubeFace getCubeFaceIndex(uint32_t target);
    static uint32_t getRenderMultisamplesCount(uint32_t numSamples);
    static ImagePlane convertPlane(ImagePlane imagePlane);
};
} // namespace NEO
