/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/surface_formats.h"

#include "drm/i915_drm.h"

namespace NEO {

struct TilingModeHelper {
    static TilingMode convert(uint32_t i915TilingMode) {
        switch (i915TilingMode) {
        case I915_TILING_X:
            return TilingMode::TILE_X;
        case I915_TILING_Y:
            return TilingMode::TILE_Y;
        case I915_TILING_NONE:
        default:
            return TilingMode::NON_TILED;
        }
    }
};

} // namespace NEO
