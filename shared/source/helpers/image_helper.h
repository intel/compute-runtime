/*
 * Copyright (C) 2025-2026 Intel Corporation
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
struct RootDeviceEnvironment;

struct ImageHelper {
    static bool areImagesCompatibleWithPackedFormat(const RootDeviceEnvironment &rootDeviceEnvironment, const ImageInfo &srcImageInfo, const ImageInfo &dstImageInfo, const GraphicsAllocation *srcAllocation, const GraphicsAllocation *dstAllocation, size_t copyWidth);
};

} // namespace NEO
