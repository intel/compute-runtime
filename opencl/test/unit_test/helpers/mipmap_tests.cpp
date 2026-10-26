/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_image.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <limits>
#include <vector>

using namespace NEO;

constexpr size_t testOrigin[]{2, 3, 5, 7};

typedef ::testing::TestWithParam<std::pair<uint32_t, size_t>> MipLevelTest;

TEST_P(MipLevelTest, givenMemObjectTypeThenProperMipLevelIsReturned) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), findMipLevel(pair.first, testOrigin));
}

INSTANTIATE_TEST_SUITE_P(MipLevel,
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

TEST_P(MipLevelOriginIdxTest, givenMemObjectTypeWhenGettingMipLevelOriginIdxThenCorrectMipLevelIsReturned) {
    auto pair = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(pair.second), getMipLevelOriginIdx(pair.first));
}

INSTANTIATE_TEST_SUITE_P(MipLevelOriginIdx,
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
        surfaceFormatInfo.surfaceFormat.imageElementSizeInBytes = 4u;
    }

    void setMipMappedImageDesc(cl_mem_object_type imageType, size_t width, size_t height, size_t depthOrArraySize, cl_uint numMipLevels) {
        imageDesc.image_type = imageType;
        imageDesc.image_width = width;
        imageDesc.image_height = height;
        imageDesc.image_depth = depthOrArraySize;
        imageDesc.image_array_size = depthOrArraySize;
        imageDesc.num_mip_levels = numMipLevels;

        const size_t rowPitch = width * surfaceFormatInfo.surfaceFormat.imageElementSizeInBytes;
        size_t slicePitch = 0;
        if (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            slicePitch = rowPitch;
        } else if (imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE3D) {
            slicePitch = rowPitch * height;
        }
        setHostPtrRowPitch(rowPitch);
        setHostPtrSlicePitch(slicePitch);
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
    image.setMipMappedImageDesc(imageType, 11, 13, 17, 16);

    auto offset = getMipOffset(&image, origin.data());

    EXPECT_EQ(expectedOffset, offset);
}

constexpr myTuple testOrigins[]{myTuple({{2, 3, 5, 7}},
                                        10800u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 2}},
                                        10684u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 1}},
                                        9724u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE3D),
                                myTuple({{2, 3, 5, 7}},
                                        12444u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 2}},
                                        11764u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 1}},
                                        9724u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE2D_ARRAY),
                                myTuple({{2, 3, 5, 0}},
                                        724u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 2, 0}},
                                        692u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 1, 0}},
                                        572u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE2D),
                                myTuple({{2, 3, 5, 0}},
                                        1360u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 2, 0}},
                                        1088u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 1, 0}},
                                        748u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE1D_ARRAY),
                                myTuple({{2, 3, 0, 0}},
                                        72u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 2, 0, 0}},
                                        64u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 1, 0, 0}},
                                        44u, CL_MEM_OBJECT_IMAGE1D),
                                myTuple({{2, 0, 0, 0}},
                                        0u, CL_MEM_OBJECT_IMAGE1D)};

INSTANTIATE_TEST_SUITE_P(MipMapOffset,
                         MipOffsetTest,
                         ::testing::ValuesIn(testOrigins));

struct MipLevelHostLayoutTestParams {
    cl_mem_object_type imageType;
    size_t width;
    size_t height;
    size_t depthOrArraySize;
    std::vector<uint32_t> expectedOffsets;
    std::vector<size_t> expectedRowPitches;
    std::vector<size_t> expectedSlicePitches;
};

using MipLevelHostLayoutTest = ::testing::TestWithParam<MipLevelHostLayoutTestParams>;

TEST_P(MipLevelHostLayoutTest, givenMipMappedImageWhenGettingMipLevelHostLayoutThenLevelsArePackedOneAfterAnotherWithLevelPitches) {
    const auto &params = GetParam();
    const auto numMipLevels = static_cast<cl_uint>(params.expectedOffsets.size());

    MockImage image;
    image.setMipMappedImageDesc(params.imageType, params.width, params.height, params.depthOrArraySize, numMipLevels);

    for (uint32_t mipLevel = 0; mipLevel < numMipLevels; mipLevel++) {
        size_t origin[4] = {};
        origin[getMipLevelOriginIdx(params.imageType)] = mipLevel;
        auto mipLevelHostLayout = image.getMipLevelHostLayout(mipLevel);

        EXPECT_EQ(params.expectedOffsets[mipLevel], getMipOffset(&image, origin)) << mipLevel;
        EXPECT_EQ(params.expectedRowPitches[mipLevel], mipLevelHostLayout.rowPitch) << mipLevel;
        EXPECT_EQ(params.expectedSlicePitches[mipLevel], mipLevelHostLayout.slicePitch) << mipLevel;
    }
}

INSTANTIATE_TEST_SUITE_P(MipLevelHostLayout,
                         MipLevelHostLayoutTest,
                         ::testing::Values(MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE2D, 8, 8, 1, {0, 256, 320, 336}, {32, 16, 8, 4}, {0, 0, 0, 0}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE2D, 1, 8, 1, {0, 32, 48, 56}, {4, 4, 4, 4}, {0, 0, 0, 0}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE2D, 8, 1, 1, {0, 32, 48, 56}, {32, 16, 8, 4}, {0, 0, 0, 0}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE1D, 8, 1, 1, {0, 32, 48, 56}, {32, 16, 8, 4}, {0, 0, 0, 0}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE1D_ARRAY, 8, 1, 3, {0, 96, 144, 168}, {32, 16, 8, 4}, {32, 16, 8, 4}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE2D_ARRAY, 64, 1, 2, {0, 512, 768, 896, 960, 992, 1008}, {256, 128, 64, 32, 16, 8, 4}, {256, 128, 64, 32, 16, 8, 4}},
                                           MipLevelHostLayoutTestParams{CL_MEM_OBJECT_IMAGE3D, 2, 2, 64, {0, 1024, 1152, 1216, 1248, 1264, 1272}, {8, 4, 4, 4, 4, 4, 4}, {16, 4, 4, 4, 4, 4, 4}}));

TEST(MipmapHelper, givenMipMappedImageWithHostLevelsLargerThan4GBWhenGettingMipOffsetThenOffsetIsNotTruncated) {
    if (is32bit) {
        GTEST_SKIP();
    }
    constexpr size_t width = 65536;
    constexpr size_t height = 65536;
    constexpr size_t arraySize = 2;
    MockImage image;
    image.setMipMappedImageDesc(CL_MEM_OBJECT_IMAGE2D_ARRAY, width, height, arraySize, 2);

    const size_t origin[4] = {0, 0, 0, 1};
    const size_t expectedOffset = width * image.getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * height * arraySize;
    EXPECT_LT(static_cast<size_t>(std::numeric_limits<uint32_t>::max()), expectedOffset);
    EXPECT_EQ(expectedOffset, getMipOffset(&image, origin));
}

TEST(MipmapHelper, givenMipMappedImageWhenGettingHostMapStorageSizeThenItCoversAllMipLevels) {
    MockImage image;
    image.setMipMappedImageDesc(CL_MEM_OBJECT_IMAGE3D, 2, 2, 64, 7);
    EXPECT_EQ(0u, image.getSize());
    EXPECT_EQ(1276u, image.getHostMapStorageSize());

    image.setMipMappedImageDesc(CL_MEM_OBJECT_IMAGE2D_ARRAY, 64, 1, 2, 7);
    EXPECT_EQ(1016u, image.getHostMapStorageSize());
}

TEST(MipmapHelper, givenNonMipMappedImageWhenGettingHostMapStorageSizeThenImageSizeIsReturned) {
    MockImage image;
    image.setMipMappedImageDesc(CL_MEM_OBJECT_IMAGE3D, 2, 2, 64, 1);
    EXPECT_EQ(image.getSize(), image.getHostMapStorageSize());
}

TEST(MipmapHelper, givenBufferWhenGettingHostMapStorageSizeThenBufferSizeIsReturned) {
    MockBuffer buffer;
    EXPECT_EQ(buffer.getSize(), buffer.getHostMapStorageSize());
}

struct MipMappedImageShape {
    size_t width;
    size_t height;
    size_t depthOrArraySize;
};

using MipMappedImageHostLayoutDisjointTest = ::testing::TestWithParam<std::tuple<cl_mem_object_type, MipMappedImageShape>>;

TEST_P(MipMappedImageHostLayoutDisjointTest, givenMipMappedImageWhenAllLevelsAndLayersAreMappedThenHostRegionsAreDisjointAndFitInHostMapStorage) {
    const auto imageType = std::get<0>(GetParam());
    const auto shape = std::get<1>(GetParam());

    const bool is1dImage = imageType == CL_MEM_OBJECT_IMAGE1D || imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY;
    const bool is3dImage = imageType == CL_MEM_OBJECT_IMAGE3D;
    const bool isArrayImage = imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY;
    const size_t height = is1dImage ? 1u : shape.height;
    const size_t depthOrArraySize = (is3dImage || isArrayImage) ? shape.depthOrArraySize : 1u;
    const size_t maxDimension = std::max({shape.width, height, is3dImage ? depthOrArraySize : 1u});
    cl_uint numMipLevels = 1;
    while ((maxDimension >> numMipLevels) > 0) {
        numMipLevels++;
    }
    if (numMipLevels < 2) {
        GTEST_SKIP();
    }

    MockImage image;
    image.setMipMappedImageDesc(imageType, shape.width, height, depthOrArraySize, numMipLevels);
    EXPECT_FALSE(image.mappingOnCpuAllowed());

    const size_t bytesPerPixel = image.getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
    std::vector<int> owner(image.getHostMapStorageSize(), -1);

    int regionId = 0;
    for (uint32_t mipLevel = 0; mipLevel < numMipLevels; mipLevel++) {
        size_t mipOrigin[4] = {};
        mipOrigin[getMipLevelOriginIdx(imageType)] = mipLevel;
        const size_t mipOffset = getMipOffset(&image, mipOrigin);
        const size_t levelWidth = std::max<size_t>(shape.width >> mipLevel, 1u);
        const size_t levelHeight = std::max<size_t>(height >> mipLevel, 1u);
        const size_t levelSlices = is3dImage ? std::max<size_t>(depthOrArraySize >> mipLevel, 1u) : depthOrArraySize;

        for (size_t slice = 0; slice < levelSlices; slice++, regionId++) {
            for (size_t row = 0; row < levelHeight; row++) {
                MemObjOffsetArray origin = {{0, row, slice}};
                if (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
                    origin = {{0, slice, 0}};
                }
                const size_t rowOffset = mipOffset + image.calculateOffsetForMapping(origin, mipLevel);
                ASSERT_LE(rowOffset + levelWidth * bytesPerPixel, owner.size());
                for (size_t byte = 0; byte < levelWidth * bytesPerPixel; byte++) {
                    ASSERT_EQ(-1, owner[rowOffset + byte]) << "mipLevel " << mipLevel << " slice " << slice << " row " << row;
                    owner[rowOffset + byte] = regionId;
                }
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(MipMappedImageHostLayout,
                         MipMappedImageHostLayoutDisjointTest,
                         ::testing::Combine(::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D,
                                                              CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D),
                                            ::testing::Values(MipMappedImageShape{8, 8, 8}, MipMappedImageShape{11, 13, 17}, MipMappedImageShape{1, 8, 3},
                                                              MipMappedImageShape{8, 1, 3}, MipMappedImageShape{64, 1, 2}, MipMappedImageShape{2, 2, 64},
                                                              MipMappedImageShape{5, 3, 7}, MipMappedImageShape{1, 1, 4})));
