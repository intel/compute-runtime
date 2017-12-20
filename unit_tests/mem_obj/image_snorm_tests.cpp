/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"
#include "gtest/gtest.h"

using namespace OCLRT;

typedef decltype(numSnormSurfaceFormats) SnormSurfaceFormatsCountType;
class GetSurfaceFormatTest : public ::testing::TestWithParam<std::tuple<size_t /*format index*/, uint64_t /*flags*/>> {
  public:
    void SetUp() override {
        size_t index;
        std::tie(index, flags) = GetParam();
        surfaceFormat = snormSurfaceFormats[index];
    }

    void compareFormats(const SurfaceFormatInfo *first, const SurfaceFormatInfo *second) {
        EXPECT_EQ(first->GenxSurfaceFormat, second->GenxSurfaceFormat);
        EXPECT_EQ(first->GMMSurfaceFormat, second->GMMSurfaceFormat);
        EXPECT_EQ(first->GMMTileWalk, second->GMMTileWalk);
        EXPECT_EQ(first->ImageElementSizeInBytes, second->ImageElementSizeInBytes);
        EXPECT_EQ(first->NumChannels, second->NumChannels);
        EXPECT_EQ(first->OCLImageFormat.image_channel_data_type, second->OCLImageFormat.image_channel_data_type);
        EXPECT_EQ(first->OCLImageFormat.image_channel_order, second->OCLImageFormat.image_channel_order);
        EXPECT_EQ(first->PerChannelSizeInBytes, second->PerChannelSizeInBytes);
    }

    SurfaceFormatInfo surfaceFormat;
    cl_mem_flags flags;
};

TEST_P(GetSurfaceFormatTest, givenSnormFormatWhenGetSurfaceFormatFromTableIsCalledThenReturnsCorrectFormat) {
    auto format = Image::getSurfaceFormatFromTable(flags, &surfaceFormat.OCLImageFormat);
    EXPECT_NE(nullptr, format);
    compareFormats(&surfaceFormat, format);
}

cl_mem_flags flagsForTests[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE};

INSTANTIATE_TEST_CASE_P(
    ImageSnormTests,
    GetSurfaceFormatTest,
    ::testing::Combine(
        ::testing::Range(static_cast<SnormSurfaceFormatsCountType>(0u), numSnormSurfaceFormats),
        ::testing::ValuesIn(flagsForTests)));

class IsSnormFormatTest : public ::testing::TestWithParam<std::tuple<uint32_t /*image data type*/, bool /*expected value*/>> {
  public:
    void SetUp() override {
        std::tie(format.image_channel_data_type, expectedValue) = GetParam();
    }

    cl_image_format format;
    bool expectedValue;
};

TEST_P(IsSnormFormatTest, givenSnormFormatWhenGetSurfaceFormatFromTableIsCalledThenReturnsCorrectFormat) {
    bool retVal = Image::isSnormFormat(format);
    EXPECT_EQ(expectedValue, retVal);
}

std::tuple<uint32_t, bool> paramsForSnormTests[] = {
    std::make_tuple<uint32_t, bool>(CL_SNORM_INT8, true),
    std::make_tuple<uint32_t, bool>(CL_SNORM_INT16, true),
    std::make_tuple<uint32_t, bool>(CL_UNORM_INT8, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_INT16, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_SHORT_565, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_SHORT_555, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_INT_101010, false),
    std::make_tuple<uint32_t, bool>(CL_SIGNED_INT8, false),
    std::make_tuple<uint32_t, bool>(CL_SIGNED_INT16, false),
    std::make_tuple<uint32_t, bool>(CL_SIGNED_INT32, false),
    std::make_tuple<uint32_t, bool>(CL_UNSIGNED_INT8, false),
    std::make_tuple<uint32_t, bool>(CL_UNSIGNED_INT16, false),
    std::make_tuple<uint32_t, bool>(CL_UNSIGNED_INT32, false),
    std::make_tuple<uint32_t, bool>(CL_HALF_FLOAT, false),
    std::make_tuple<uint32_t, bool>(CL_FLOAT, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_INT24, false),
    std::make_tuple<uint32_t, bool>(CL_UNORM_INT_101010_2, false)};

INSTANTIATE_TEST_CASE_P(
    ImageSnormTests,
    IsSnormFormatTest,
    ::testing::ValuesIn(paramsForSnormTests));
