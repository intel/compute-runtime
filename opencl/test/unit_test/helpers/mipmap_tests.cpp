/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_image.h"

#include "gtest/gtest.h"

using namespace NEO;

constexpr size_t testOrigin[]{2, 3, 5, 7};

typedef ::testing::TestWithParam<std::pair<uint32_t, size_t>> MipLevelTest;

TEST_P(MipLevelTest, givenMemObjectTypeReturnProperMipLevel) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), findMipLevel(pair.first, testOrigin));
}

INSTANTIATE_TEST_CASE_P(MipLevel,
                        MipLevelTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, testOrigin[1]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, testOrigin[2]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, testOrigin[2]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, testOrigin[3]),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, testOrigin[3]),
                                          std::make_pair(CL_MEM_OBJECT_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_PIPE, 0U)));

typedef ::testing::TestWithParam<std::pair<uint32_t, uint32_t>> MipLevelOriginIdxTest;

TEST_P(MipLevelOriginIdxTest, givenMemObjectTypeReturnProperMipLevelOriginIdx) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), getMipLevelOriginIdx(pair.first));
}

INSTANTIATE_TEST_CASE_P(MipLevelOriginIdx,
                        MipLevelOriginIdxTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, 1U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, 2U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, 2U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, 3U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, 3U),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_BUFFER, 0U),
                                          std::make_pair(CL_MEM_OBJECT_BUFFER, static_cast<uint32_t>(-1)),
                                          std::make_pair(CL_MEM_OBJECT_PIPE, static_cast<uint32_t>(-1))));

TEST(MipmapHelper, givenClImageDescWithoutMipLevelsWhenIsMipMappedIsCalledThenFalseIsReturned) {
    cl_image_desc desc = {};
    desc.num_mip_levels = 0;
    EXPECT_FALSE(NEO::isMipMapped(desc));
    desc.num_mip_levels = 1;
    EXPECT_FALSE(NEO::isMipMapped(desc));
}

TEST(MipmapHelper, givenClImageDescWithMipLevelsWhenIsMipMappedIsCalledThenTrueIsReturned) {
    cl_image_desc desc = {};
    desc.num_mip_levels = 2;
    EXPECT_TRUE(NEO::isMipMapped(desc));
}

TEST(MipmapHelper, givenBufferWhenIsMipMappedIsCalledThenFalseIsReturned) {
    MockBuffer buffer;
    EXPECT_FALSE(NEO::isMipMapped(&buffer));
}

struct MockImage : MockImageBase {

    MockImage() : MockImageBase() {
        surfaceFormatInfo.surfaceFormat.ImageElementSizeInBytes = 4u;
    }
};

TEST(MipmapHelper, givenImageWithoutMipLevelsWhenIsMipMappedIsCalledThenFalseIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 0;
    EXPECT_FALSE(NEO::isMipMapped(&image));
    image.imageDesc.num_mip_levels = 1;
    EXPECT_FALSE(NEO::isMipMapped(&image));
}

TEST(MipmapHelper, givenImageWithMipLevelsWhenIsMipMappedIsCalledThenTrueIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 2;
    EXPECT_TRUE(NEO::isMipMapped(&image));
}

TEST(MipmapHelper, givenImageWithoutMipLevelsWhenGetMipOffsetIsCalledThenZeroIsReturned) {
    MockImage image;
    image.imageDesc.num_mip_levels = 1;

    auto offset = getMipOffset(&image, testOrigin);

    EXPECT_EQ(0U, offset);
}

using myTuple = std::tuple<std::array<size_t, 4>, uint32_t, uint32_t>;
using MipOffsetTest = ::testing::TestWithParam<myTuple>;

TEST_P(MipOffsetTest, givenImageWithMipLevelsWhenGetMipOffsetIsCalledThenProperOffsetIsReturned) {
    std::array<size_t, 4> origin;
    uint32_t expectedOffset;
    cl_mem_object_type imageType;
    std::tie(origin, expectedOffset, imageType) = GetParam();

    MockImage image;
    image.imageDesc.num_mip_levels = 16;
    image.imageDesc.image_type = imageType;
    image.imageDesc.image_width = 11;
    image.imageDesc.image_height = 13;
    image.imageDesc.image_depth = 17;

    auto offset = getMipOffset(&image, origin.data());

    EXPECT_EQ(expectedOffset, offset);
}

constexpr myTuple testOrigins[]{myTuple({{2, 3, 5, 7}},
                                        812u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 2}},
                                        592u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 1}},
                                        572u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 7}},
                                        812u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 2}},
                                        592u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 1}},
                                        572u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 0}},
                                        724u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 2, 0}},
                                        592u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 1, 0}},
                                        572u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 5, 0}},
                                        724u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 2, 0}},
                                        592u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 1, 0}},
                                        572u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 0, 0}},
                                        56u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 2, 0, 0}},
                                        52u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 1, 0, 0}},
                                        44u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 0, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE1D)};

INSTANTIATE_TEST_CASE_P(MipMapOffset,
                        MipOffsetTest,
                        ::testing::ValuesIn(testOrigins));
