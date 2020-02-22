/*
 * Copyright (C) 2018-2020 Intel Corporation
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

TEST_P(HasAlphaChannelTest, imageFormatHasAlphaChannel) {
    cl_image_format imageFormat;
    bool expectedValue;
    std::tie(imageFormat.image_channel_order, expectedValue) = GetParam();
    EXPECT_EQ(expectedValue, MockImage::hasAlphaChannel(&imageFormat));
}

std::tuple<uint32_t, bool> paramsForAlphaChannelTests[] = {
    std::make_tuple<uint32_t, bool>(CL_R, false),
    std::make_tuple<uint32_t, bool>(CL_A, true),
    std::make_tuple<uint32_t, bool>(CL_RG, false),
    std::make_tuple<uint32_t, bool>(CL_RA, true),
    std::make_tuple<uint32_t, bool>(CL_RGB, false),
    std::make_tuple<uint32_t, bool>(CL_RGBA, true),
    std::make_tuple<uint32_t, bool>(CL_BGRA, true),
    std::make_tuple<uint32_t, bool>(CL_ARGB, true),
    std::make_tuple<uint32_t, bool>(CL_INTENSITY, true),
    std::make_tuple<uint32_t, bool>(CL_LUMINANCE, false),
    std::make_tuple<uint32_t, bool>(CL_Rx, true),
    std::make_tuple<uint32_t, bool>(CL_RGx, true),
    std::make_tuple<uint32_t, bool>(CL_RGBx, true),
    std::make_tuple<uint32_t, bool>(CL_DEPTH, false),
    std::make_tuple<uint32_t, bool>(CL_DEPTH_STENCIL, false),
    std::make_tuple<uint32_t, bool>(CL_sRGB, false),
    std::make_tuple<uint32_t, bool>(CL_sRGBx, true),
    std::make_tuple<uint32_t, bool>(CL_sRGBA, true),
    std::make_tuple<uint32_t, bool>(CL_sBGRA, true),
    std::make_tuple<uint32_t, bool>(CL_ABGR, true),
    std::make_tuple<uint32_t, bool>(CL_NV12_INTEL, false),
    std::make_tuple<uint32_t, bool>(CL_YUYV_INTEL, false),
    std::make_tuple<uint32_t, bool>(CL_UYVY_INTEL, false),
    std::make_tuple<uint32_t, bool>(CL_YVYU_INTEL, false),
    std::make_tuple<uint32_t, bool>(CL_VYUY_INTEL, false)};

INSTANTIATE_TEST_CASE_P(
    ImageFormatTests,
    HasAlphaChannelTest,
    ::testing::ValuesIn(paramsForAlphaChannelTests));
