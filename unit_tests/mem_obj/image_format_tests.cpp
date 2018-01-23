/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/mem_obj/image.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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
