/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include <array>

using namespace OCLRT;

const cl_mem_flags flagsForTests[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE};

const std::tuple<const SurfaceFormatInfo *, const size_t *> paramsForSnormTests[] = {
    std::make_tuple(readOnlySurfaceFormats, &numReadOnlySurfaceFormats),
    std::make_tuple(writeOnlySurfaceFormats, &numWriteOnlySurfaceFormats),
    std::make_tuple(readWriteSurfaceFormats, &numReadWriteSurfaceFormats),
};

const std::array<SurfaceFormatInfo, 6> referenceSnormSurfaceFormats = {{
    // clang-format off
    {{CL_R, CL_SNORM_INT8},     GMM_FORMAT_R8_SNORM_TYPE,           GFX3DSTATE_SURFACEFORMAT_R8_SNORM,           0, 1, 1, 1},
    {{CL_R, CL_SNORM_INT16},    GMM_FORMAT_R16_SNORM_TYPE,          GFX3DSTATE_SURFACEFORMAT_R16_SNORM,          0, 1, 2, 2},
    {{CL_RG, CL_SNORM_INT8},    GMM_FORMAT_R8G8_SNORM_TYPE,         GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM,         0, 2, 1, 2},
    {{CL_RG, CL_SNORM_INT16},   GMM_FORMAT_R16G16_SNORM_TYPE,       GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM,       0, 2, 2, 4},
    {{CL_RGBA, CL_SNORM_INT8},  GMM_FORMAT_R8G8B8A8_SNORM_TYPE,     GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM,     0, 4, 1, 4},
    {{CL_RGBA, CL_SNORM_INT16}, GMM_FORMAT_R16G16B16A16_SNORM_TYPE, GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM, 0, 4, 2, 8},
    // clang-format on
}};

using SnormSurfaceFormatAccessFlagsTests = ::testing::TestWithParam<uint64_t /*flags*/>;

TEST_P(SnormSurfaceFormatAccessFlagsTests, givenSnormFormatWhenGetSurfaceFormatFromTableIsCalledThenReturnsCorrectFormat) {
    EXPECT_EQ(6u, referenceSnormSurfaceFormats.size());
    cl_mem_flags flags = GetParam();

    for (size_t i = 0u; i < referenceSnormSurfaceFormats.size(); i++) {
        const auto &snormSurfaceFormat = referenceSnormSurfaceFormats[i];
        auto format = Image::getSurfaceFormatFromTable(flags, &snormSurfaceFormat.OCLImageFormat);
        EXPECT_NE(nullptr, format);
        EXPECT_TRUE(memcmp(&snormSurfaceFormat, format, sizeof(SurfaceFormatInfo)) == 0);
    }
}

using SnormSurfaceFormatTests = ::testing::TestWithParam<std::tuple<const SurfaceFormatInfo *, const size_t *>>;

TEST_P(SnormSurfaceFormatTests, givenSnormOclFormatWhenCheckingrReadOnlySurfaceFormatsThenFindExactCount) {
    const SurfaceFormatInfo *formatsTable = std::get<0>(GetParam());
    size_t formatsTableCount = *std::get<1>(GetParam());

    size_t snormFormatsFound = 0;
    for (size_t i = 0; i < formatsTableCount; i++) {
        auto oclFormat = formatsTable[i].OCLImageFormat;
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
