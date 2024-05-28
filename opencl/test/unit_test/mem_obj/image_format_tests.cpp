/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MockImage : public Image {
    using Image::hasAlphaChannel;
};

typedef ::testing::TestWithParam<std::tuple<uint32_t /*channel order*/,
                                            bool /*has alpha channel*/>>
    HasAlphaChannelTest;

TEST_P(HasAlphaChannelTest, GivenImageFormatWhenCheckingForAlphaChannelThenReturnCorrectValue) {
    cl_image_format imageFormat;
    bool expectedValue;
    std::tie(imageFormat.image_channel_order, expectedValue) = GetParam();
    EXPECT_EQ(expectedValue, MockImage::hasAlphaChannel(&imageFormat));
}

std::tuple<uint32_t, bool> paramsForAlphaChannelTests[] = {
    {CL_R, false},
    {CL_A, true},
    {CL_RG, false},
    {CL_RA, true},
    {CL_RGB, false},
    {CL_RGBA, true},
    {CL_BGRA, true},
    {CL_ARGB, true},
    {CL_INTENSITY, true},
    {CL_LUMINANCE, false},
    {CL_Rx, true},
    {CL_RGx, true},
    {CL_RGBx, true},
    {CL_DEPTH, false},
    {CL_DEPTH_STENCIL, false},
    {CL_sRGB, false},
    {CL_sRGBx, true},
    {CL_sRGBA, true},
    {CL_sBGRA, true},
    {CL_ABGR, true},
    {CL_NV12_INTEL, false},
    {CL_YUYV_INTEL, false},
    {CL_UYVY_INTEL, false},
    {CL_YVYU_INTEL, false},
    {CL_VYUY_INTEL, false}};

INSTANTIATE_TEST_SUITE_P(
    ImageFormatTests,
    HasAlphaChannelTest,
    ::testing::ValuesIn(paramsForAlphaChannelTests));
