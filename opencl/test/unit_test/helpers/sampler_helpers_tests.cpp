/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/sampler_helpers.h"

#include "gtest/gtest.h"

TEST(SamplerHelpers, WhenGettingAddrModeEnumsThenCorrectValuesAreReturned) {
    EXPECT_EQ(CLK_ADDRESS_REPEAT, getAddrModeEnum(CL_ADDRESS_REPEAT));
    EXPECT_EQ(CLK_ADDRESS_CLAMP_TO_EDGE, getAddrModeEnum(CL_ADDRESS_CLAMP_TO_EDGE));
    EXPECT_EQ(CLK_ADDRESS_CLAMP, getAddrModeEnum(CL_ADDRESS_CLAMP));
    EXPECT_EQ(CLK_ADDRESS_NONE, getAddrModeEnum(CL_ADDRESS_NONE));
    EXPECT_EQ(CLK_ADDRESS_MIRRORED_REPEAT, getAddrModeEnum(CL_ADDRESS_MIRRORED_REPEAT));
}

TEST(SamplerHelpers, WhenGettingNormCoordsEnumsThenCorrectValuesAreReturned) {
    EXPECT_EQ(CLK_NORMALIZED_COORDS_TRUE, getNormCoordsEnum(true));
    EXPECT_EQ(CLK_NORMALIZED_COORDS_FALSE, getNormCoordsEnum(false));
}
