/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"

#include "gtest/gtest.h"

#include <array>

using namespace NEO;

const cl_mem_flags flagsForTests[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE};

const ArrayRef<const ClSurfaceFormatInfo> paramsForSnormTests[] = {
    SurfaceFormats::readOnly12(),
    SurfaceFormats::readOnly20(),
    SurfaceFormats::writeOnly(),
    SurfaceFormats::readWrite()};

const std::array<ClSurfaceFormatInfo, 6> referenceSnormSurfaceFormats = {{
    // clang-format off
    {{CL_R, CL_SNORM_INT8},     {GMM_FORMAT_R8_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8_SNORM,           0, 1, 1, 1}},
    {{CL_R, CL_SNORM_INT16},    {GMM_FORMAT_R16_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R16_SNORM,          0, 1, 2, 2}},
    {{CL_RG, CL_SNORM_INT8},    {GMM_FORMAT_R8G8_SNORM_TYPE,         GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM,         0, 2, 1, 2}},
    {{CL_RG, CL_SNORM_INT16},   {GMM_FORMAT_R16G16_SNORM_TYPE,       GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM,       0, 2, 2, 4}},
    {{CL_RGBA, CL_SNORM_INT8},  {GMM_FORMAT_R8G8B8A8_SNORM_TYPE,     GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM,     0, 4, 1, 4}},
    {{CL_RGBA, CL_SNORM_INT16}, {GMM_FORMAT_R16G16B16A16_SNORM_TYPE, GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM, 0, 4, 2, 8}},
    // clang-format on
}};

using SnormSurfaceFormatAccessFlagsTests = ::testing::TestWithParam<uint64_t /*flags*/>;

TEST_P(SnormSurfaceFormatAccessFlagsTests, givenSnormFormatWhenGetSurfaceFormatFromTableIsCalledThenReturnsCorrectFormat) {
    EXPECT_EQ(6u, referenceSnormSurfaceFormats.size());
    cl_mem_flags flags = GetParam();

    for (const auto &snormSurfaceFormat : referenceSnormSurfaceFormats) {
        auto format = Image::getSurfaceFormatFromTable(flags, &snormSurfaceFormat.OCLImageFormat, 12);
        EXPECT_NE(nullptr, format);
        EXPECT_TRUE(memcmp(&snormSurfaceFormat, format, sizeof(ClSurfaceFormatInfo)) == 0);
    }
    for (const auto &snormSurfaceFormat : referenceSnormSurfaceFormats) {
        auto format = Image::getSurfaceFormatFromTable(flags, &snormSurfaceFormat.OCLImageFormat, 20);
        EXPECT_NE(nullptr, format);
        EXPECT_TRUE(memcmp(&snormSurfaceFormat, format, sizeof(ClSurfaceFormatInfo)) == 0);
    }
}

using SnormSurfaceFormatTests = ::testing::TestWithParam<ArrayRef<const ClSurfaceFormatInfo>>;

TEST_P(SnormSurfaceFormatTests, givenSnormOclFormatWhenCheckingrReadOnlySurfaceFormatsThenFindExactCount) {
    ArrayRef<const ClSurfaceFormatInfo> formatsTable = GetParam();

    size_t snormFormatsFound = 0;
    for (const auto &format : formatsTable) {
        auto oclFormat = format.OCLImageFormat;
        if (CL_SNORM_INT8 == oclFormat.image_channel_data_type || CL_SNORM_INT16 == oclFormat.image_channel_data_type) {
            EXPECT_TRUE(oclFormat.image_channel_order == CL_R || oclFormat.image_channel_order == CL_RG || oclFormat.image_channel_order == CL_RGBA);
            snormFormatsFound++;
        }
    }
    EXPECT_EQ(6u, snormFormatsFound);
}

INSTANTIATE_TEST_CASE_P(
    ImageSnormTests,
    SnormSurfaceFormatAccessFlagsTests,
    ::testing::ValuesIn(flagsForTests));

INSTANTIATE_TEST_CASE_P(
    ImageSnormTests,
    SnormSurfaceFormatTests,
    ::testing::ValuesIn(paramsForSnormTests));
