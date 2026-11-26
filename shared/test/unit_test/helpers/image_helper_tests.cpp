/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/image_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ImageHelperTest, givenViewDepthAndMipLevelWhenGettingVolumeDepthForMipViewThenViewDepthIsScaledByMipLevel) {
    EXPECT_EQ(2u, ImageHelper::getVolumeDepthForMipView(2048u, 2u, 0u));
    EXPECT_EQ(8u, ImageHelper::getVolumeDepthForMipView(2048u, 2u, 2u));
}

TEST(ImageHelperTest, givenScaledViewDepthAboveMaxDepthWhenGettingVolumeDepthForMipViewThenMaxDepthIsReturned) {
    EXPECT_EQ(6u, ImageHelper::getVolumeDepthForMipView(6u, 2u, 2u));
    EXPECT_EQ(2048u, ImageHelper::getVolumeDepthForMipView(2048u, 0x80000000u, 31u));
}
