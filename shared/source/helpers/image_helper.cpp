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

} // namespace NEO
