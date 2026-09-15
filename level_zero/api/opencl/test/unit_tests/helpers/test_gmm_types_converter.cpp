/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_gmm_types_converter.h"

namespace NEO {
namespace LEO {
namespace ult {

constexpr uint32_t glTextureCubeMapPositiveX = 0x8515;
constexpr uint32_t glTextureCubeMapNegativeX = 0x8516;
constexpr uint32_t glTextureCubeMapPositiveY = 0x8517;
constexpr uint32_t glTextureCubeMapNegativeY = 0x8518;
constexpr uint32_t glTextureCubeMapPositiveZ = 0x8519;
constexpr uint32_t glTextureCubeMapNegativeZ = 0x851A;
constexpr uint32_t glTexture2d = 0x0DE1;
constexpr uint32_t glTexture3d = 0x806F;

TEST(GmmTypesConverterMultisamplesTests, givenSupportedSampleCountsWhenGetRenderMultisamplesCountThenReturnsEncodedValue) {
    EXPECT_EQ(1u, GmmTypesConverter::getRenderMultisamplesCount(2));
    EXPECT_EQ(2u, GmmTypesConverter::getRenderMultisamplesCount(4));
    EXPECT_EQ(3u, GmmTypesConverter::getRenderMultisamplesCount(8));
    EXPECT_EQ(4u, GmmTypesConverter::getRenderMultisamplesCount(16));
}

TEST(GmmTypesConverterMultisamplesTests, givenUnsupportedSampleCountsWhenGetRenderMultisamplesCountThenReturnsZero) {
    EXPECT_EQ(0u, GmmTypesConverter::getRenderMultisamplesCount(0));
    EXPECT_EQ(0u, GmmTypesConverter::getRenderMultisamplesCount(1));
    EXPECT_EQ(0u, GmmTypesConverter::getRenderMultisamplesCount(3));
    EXPECT_EQ(0u, GmmTypesConverter::getRenderMultisamplesCount(5));
    EXPECT_EQ(0u, GmmTypesConverter::getRenderMultisamplesCount(32));
}

TEST(GmmTypesConverterConvertPlaneTests, givenPlaneUVWhenConvertPlaneThenReturnsPlaneU) {
    EXPECT_EQ(ImagePlane::planeU, GmmTypesConverter::convertPlane(ImagePlane::planeUV));
}

TEST(GmmTypesConverterConvertPlaneTests, givenAnyOtherPlaneWhenConvertPlaneThenReturnsSamePlane) {
    EXPECT_EQ(ImagePlane::noPlane, GmmTypesConverter::convertPlane(ImagePlane::noPlane));
    EXPECT_EQ(ImagePlane::planeY, GmmTypesConverter::convertPlane(ImagePlane::planeY));
    EXPECT_EQ(ImagePlane::planeU, GmmTypesConverter::convertPlane(ImagePlane::planeU));
    EXPECT_EQ(ImagePlane::planeV, GmmTypesConverter::convertPlane(ImagePlane::planeV));
}

TEST(GmmTypesConverterCubeFaceTests, givenCubeMapTargetsWhenGetCubeFaceIndexThenReturnsMatchingFace) {
    EXPECT_EQ(__GMM_CUBE_FACE_NEG_X, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapNegativeX));
    EXPECT_EQ(__GMM_CUBE_FACE_POS_X, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapPositiveX));
    EXPECT_EQ(__GMM_CUBE_FACE_NEG_Y, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapNegativeY));
    EXPECT_EQ(__GMM_CUBE_FACE_POS_Y, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapPositiveY));
    EXPECT_EQ(__GMM_CUBE_FACE_NEG_Z, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapNegativeZ));
    EXPECT_EQ(__GMM_CUBE_FACE_POS_Z, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapPositiveZ));
}

TEST(GmmTypesConverterCubeFaceTests, givenNonCubeMapTargetWhenGetCubeFaceIndexThenReturnsNoCubeMap) {
    EXPECT_EQ(__GMM_NO_CUBE_MAP, GmmTypesConverter::getCubeFaceIndex(glTexture2d));
    EXPECT_EQ(__GMM_NO_CUBE_MAP, GmmTypesConverter::getCubeFaceIndex(glTexture3d));
    EXPECT_EQ(__GMM_NO_CUBE_MAP, GmmTypesConverter::getCubeFaceIndex(0u));
    EXPECT_EQ(__GMM_NO_CUBE_MAP, GmmTypesConverter::getCubeFaceIndex(glTextureCubeMapNegativeZ + 1u));
}

TEST(GmmTypesConverterQueryImgFromBufferParamsTests, givenNonZeroRowPitchWhenQueryImgFromBufferParamsThenDescriptorPitchIsUsed) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.imageElementSizeInBytes = 4;

    ImageInfo imgInfo{};
    imgInfo.surfaceFormat = &surfaceFormat;
    imgInfo.imgDesc.imageRowPitch = 512;
    imgInfo.imgDesc.imageWidth = 64;
    imgInfo.imgDesc.imageHeight = 8;
    imgInfo.qPitch = 123;

    MockGraphicsAllocation allocation(nullptr, 4096u);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &allocation);

    EXPECT_EQ(512u, imgInfo.rowPitch);
    EXPECT_EQ(512u * 8u, imgInfo.slicePitch);
    EXPECT_EQ(4096u, imgInfo.size);
    EXPECT_EQ(0u, imgInfo.qPitch);
}

TEST(GmmTypesConverterQueryImgFromBufferParamsTests, givenZeroRowPitchWhenQueryImgFromBufferParamsThenPitchIsDerivedFromWidth) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.imageElementSizeInBytes = 4;

    ImageInfo imgInfo{};
    imgInfo.surfaceFormat = &surfaceFormat;
    imgInfo.imgDesc.imageRowPitch = 0;
    imgInfo.imgDesc.imageWidth = 64;
    imgInfo.imgDesc.imageHeight = 8;

    MockGraphicsAllocation allocation(nullptr, 2048u);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &allocation);

    EXPECT_EQ(64u * 4u, imgInfo.rowPitch);
    EXPECT_EQ(64u * 4u * 8u, imgInfo.slicePitch);
    EXPECT_EQ(2048u, imgInfo.size);
}

TEST(GmmTypesConverterQueryImgFromBufferParamsTests, givenZeroWidthAndHeightWhenQueryImgFromBufferParamsThenValidParamDefaultsAreUsed) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.imageElementSizeInBytes = 2;

    ImageInfo imgInfo{};
    imgInfo.surfaceFormat = &surfaceFormat;
    imgInfo.imgDesc.imageRowPitch = 0;
    imgInfo.imgDesc.imageWidth = 0;
    imgInfo.imgDesc.imageHeight = 0;

    MockGraphicsAllocation allocation(nullptr, 64u);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &allocation);

    EXPECT_EQ(2u, imgInfo.rowPitch);
    EXPECT_EQ(2u, imgInfo.slicePitch);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
