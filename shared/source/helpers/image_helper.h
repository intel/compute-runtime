/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NEO {
struct ImageInfo;
class GraphicsAllocation;
class ProductHelper;

struct ImageHelper {
    static bool areImagesCompatibleWithPackedFormat(const ProductHelper &productHelper, const ImageInfo &srcImageInfo, const ImageInfo &dstImageInfo, const GraphicsAllocation *srcAllocation, const GraphicsAllocation *dstAllocation, size_t copyWidth);
};

} // namespace NEO