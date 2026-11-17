/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/image_helper.h"

namespace NEO {

bool ImageHelper::areImagesCompatibleWithPackedFormat(const RootDeviceEnvironment &rootDeviceEnvironment, const ImageInfo &srcImageInfo, const ImageInfo &dstImageInfo, const GraphicsAllocation *srcAllocation, const GraphicsAllocation *dstAllocation, size_t copyWidth) {
    return false;
}

uint64_t ImageHelper::getMultisampleControlSurfaceAddress(const GraphicsAllocation &imageAllocation, const GraphicsAllocation *mcsAllocation, bool isUnifiedMcsSurface) {
    return 0u;
}

bool ImageHelper::getReducedMultisampleSurfaceLayout(const Gmm *gmm, uint64_t surfaceAddress, uint64_t mcsSurfaceAddress, ReducedMultisampleSurfaceLayout &layout) {
    return false;
}

bool ImageHelper::isMultisampleControlSurfaceLayoutSupported(const GraphicsAllocation &imageAllocation, const GraphicsAllocation *mcsAllocation, bool isUnifiedMcsSurface, uint64_t surfaceOffset) {
    return false;
}

} // namespace NEO
