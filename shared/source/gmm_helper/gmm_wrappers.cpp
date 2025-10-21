/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/surface_format_info.h"

namespace NEO {
static_assert(static_cast<GMM_YUV_PLANE>(ImagePlane::noPlane) == GMM_NO_PLANE);
static_assert(static_cast<GMM_YUV_PLANE>(ImagePlane::planeY) == GMM_PLANE_Y);
static_assert(static_cast<GMM_YUV_PLANE>(ImagePlane::planeU) == GMM_PLANE_U);
static_assert(static_cast<GMM_YUV_PLANE>(ImagePlane::planeV) == GMM_PLANE_V);

static_assert(static_cast<GMM_CUBE_FACE_ENUM>(gmmNoCubeMap) == __GMM_NO_CUBE_MAP);

static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::tiledX) == GMM_TILED_X);
static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::tiledY) == GMM_TILED_Y);
static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::tiledW) == GMM_TILED_W);
static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::notTiled) == GMM_NOT_TILED);
static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::tiled4) == GMM_TILED_4);
static_assert(static_cast<GMM_TILE_TYPE>(ImageTilingMode::tiled64) == GMM_TILED_64);
} // namespace NEO
