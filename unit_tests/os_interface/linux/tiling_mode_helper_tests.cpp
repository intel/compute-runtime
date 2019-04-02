/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/tiling_mode_helper.h"

#include "gtest/gtest.h"
using namespace NEO;

TEST(TilingModeHelper, givenI915ValuesWhenConvertingCorrectTilingModesAreReturned) {
    EXPECT_EQ(TilingMode::NON_TILED, TilingModeHelper::convert(I915_TILING_NONE));
    EXPECT_EQ(TilingMode::TILE_Y, TilingModeHelper::convert(I915_TILING_Y));
    EXPECT_EQ(TilingMode::TILE_X, TilingModeHelper::convert(I915_TILING_X));
    EXPECT_EQ(TilingMode::NON_TILED, TilingModeHelper::convert(I915_TILING_LAST + 1));
}
