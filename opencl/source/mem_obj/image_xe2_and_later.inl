/*
 * Copyright (C) 2024 Intel Corporation
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
} // namespace NEO