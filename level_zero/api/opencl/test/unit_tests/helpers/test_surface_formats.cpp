/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(SurfaceFormatsTests, givenEveryTableWhenQueriedThenItIsNonEmpty) {
    EXPECT_FALSE(SurfaceFormats::readOnly().empty());
    EXPECT_FALSE(SurfaceFormats::writeOnly().empty());
    EXPECT_FALSE(SurfaceFormats::readWrite().empty());
    EXPECT_FALSE(SurfaceFormats::packedYuv().empty());
    EXPECT_FALSE(SurfaceFormats::planarYuv().empty());
    EXPECT_FALSE(SurfaceFormats::packed().empty());
    EXPECT_FALSE(SurfaceFormats::readOnlyDepth().empty());
    EXPECT_FALSE(SurfaceFormats::readWriteDepth().empty());
}

TEST(SurfaceFormatsTests, givenReadOnlyTableWhenQueriedThenItIsASupersetOfReadWrite) {
    EXPECT_GT(SurfaceFormats::readOnly().size(), SurfaceFormats::readWrite().size());
    EXPECT_EQ(SurfaceFormats::writeOnly().size(), SurfaceFormats::readWrite().size());
}

TEST(SurfaceFormatsTests, givenReadOnlyDepthTableWhenQueriedThenItIsASupersetOfReadWriteDepth) {
    EXPECT_GT(SurfaceFormats::readOnlyDepth().size(), SurfaceFormats::readWriteDepth().size());
}

TEST(SurfaceFormatsTests, givenReadOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsReadOnlyTable) {
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY);
    EXPECT_EQ(SurfaceFormats::readOnly().begin(), formats.begin());
    EXPECT_EQ(SurfaceFormats::readOnly().size(), formats.size());
}

TEST(SurfaceFormatsTests, givenWriteOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsWriteOnlyTable) {
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_WRITE_ONLY);
    EXPECT_EQ(SurfaceFormats::writeOnly().begin(), formats.begin());
    EXPECT_EQ(SurfaceFormats::writeOnly().size(), formats.size());
}

TEST(SurfaceFormatsTests, givenReadWriteFlagWhenSelectingBySurfaceFormatsThenReturnsReadWriteTable) {
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_WRITE);
    EXPECT_EQ(SurfaceFormats::readWrite().begin(), formats.begin());
    EXPECT_EQ(SurfaceFormats::readWrite().size(), formats.size());
}

TEST(SurfaceFormatsTests, givenNoAccessFlagWhenSelectingBySurfaceFormatsThenReturnsReadWriteTable) {
    auto formats = SurfaceFormats::surfaceFormats(0);
    EXPECT_EQ(SurfaceFormats::readWrite().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenBothReadOnlyAndWriteOnlyFlagsWhenSelectingBySurfaceFormatsThenReadOnlyWins) {
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY);
    EXPECT_EQ(SurfaceFormats::readOnly().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenNV12FormatWhenSelectingBySurfaceFormatsThenReturnsPlanarYuvTableRegardlessOfFlags) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(0), static_cast<cl_mem_flags>(CL_MEM_READ_ONLY),
                               static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY), static_cast<cl_mem_flags>(CL_MEM_READ_WRITE)}) {
        auto formats = SurfaceFormats::surfaceFormats(flags, &format);
        EXPECT_EQ(SurfaceFormats::planarYuv().begin(), formats.begin());
    }
}

TEST(SurfaceFormatsTests, givenPackedYuvFormatWhenSelectingBySurfaceFormatsThenReturnsPackedYuvTable) {
    for (cl_channel_order channelOrder : {CL_YUYV_INTEL, CL_UYVY_INTEL, CL_YVYU_INTEL, CL_VYUY_INTEL}) {
        cl_image_format format{channelOrder, CL_UNORM_INT8};
        auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY, &format);
        EXPECT_EQ(SurfaceFormats::packedYuv().begin(), formats.begin());
    }
}

TEST(SurfaceFormatsTests, givenDepthFormatWithReadOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsReadOnlyDepthTable) {
    cl_image_format format{CL_DEPTH, CL_FLOAT};
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY, &format);
    EXPECT_EQ(SurfaceFormats::readOnlyDepth().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenDepthStencilFormatWithReadOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsReadOnlyDepthTable) {
    cl_image_format format{CL_DEPTH_STENCIL, CL_UNORM_INT24};
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY, &format);
    EXPECT_EQ(SurfaceFormats::readOnlyDepth().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenDepthFormatWithoutReadOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsReadWriteDepthTable) {
    cl_image_format format{CL_DEPTH, CL_UNORM_INT16};
    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(0), static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY),
                               static_cast<cl_mem_flags>(CL_MEM_READ_WRITE)}) {
        auto formats = SurfaceFormats::surfaceFormats(flags, &format);
        EXPECT_EQ(SurfaceFormats::readWriteDepth().begin(), formats.begin());
    }
}

TEST(SurfaceFormatsTests, givenPlainFormatWithReadOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsReadOnlyTable) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_READ_ONLY, &format);
    EXPECT_EQ(SurfaceFormats::readOnly().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenPlainFormatWithWriteOnlyFlagWhenSelectingBySurfaceFormatsThenReturnsWriteOnlyTable) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    auto formats = SurfaceFormats::surfaceFormats(CL_MEM_WRITE_ONLY, &format);
    EXPECT_EQ(SurfaceFormats::writeOnly().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenPlainFormatWithoutAccessFlagsWhenSelectingBySurfaceFormatsThenReturnsReadWriteTable) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    auto formats = SurfaceFormats::surfaceFormats(0, &format);
    EXPECT_EQ(SurfaceFormats::readWrite().begin(), formats.begin());
}

TEST(SurfaceFormatsTests, givenSrgbFormatWhenSelectingBySurfaceFormatsThenOnlyReadOnlyTableContainsIt) {
    cl_image_format format{CL_sRGBA, CL_UNORM_INT8};
    auto contains = [&format](ArrayRef<const ClSurfaceFormatInfo> formats) {
        for (const auto &entry : formats) {
            if (entry.oclImageFormat.image_channel_order == format.image_channel_order &&
                entry.oclImageFormat.image_channel_data_type == format.image_channel_data_type) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(contains(SurfaceFormats::readOnly()));
    EXPECT_FALSE(contains(SurfaceFormats::writeOnly()));
    EXPECT_FALSE(contains(SurfaceFormats::readWrite()));
}

TEST(SurfaceFormatsTests, givenEveryEntryWhenInspectedThenChannelCountAndSizeAreConsistent) {
    const ArrayRef<const ClSurfaceFormatInfo> tables[] = {
        SurfaceFormats::readOnly(), SurfaceFormats::writeOnly(), SurfaceFormats::readWrite(),
        SurfaceFormats::readOnlyDepth(), SurfaceFormats::readWriteDepth()};

    for (const auto &table : tables) {
        for (const auto &entry : table) {
            EXPECT_GT(entry.surfaceFormat.numChannels, 0u);
            EXPECT_GT(entry.surfaceFormat.perChannelSizeInBytes, 0u);
            EXPECT_EQ(entry.surfaceFormat.numChannels * entry.surfaceFormat.perChannelSizeInBytes,
                      entry.surfaceFormat.imageElementSizeInBytes);
        }
    }
}

TEST(SurfaceFormatsIsImage2dTests, givenImageTypesWhenCheckingIsImage2dThenOnlyImage2dMatches) {
    EXPECT_TRUE(SurfaceFormats::isImage2d(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_FALSE(SurfaceFormats::isImage2d(CL_MEM_OBJECT_IMAGE2D_ARRAY));
    EXPECT_FALSE(SurfaceFormats::isImage2d(CL_MEM_OBJECT_IMAGE3D));
    EXPECT_FALSE(SurfaceFormats::isImage2d(CL_MEM_OBJECT_IMAGE1D));
    EXPECT_FALSE(SurfaceFormats::isImage2d(CL_MEM_OBJECT_BUFFER));
}

TEST(SurfaceFormatsIsImage2dTests, givenImageTypesWhenCheckingIsImage2dOr2dArrayThenBothMatch) {
    EXPECT_TRUE(SurfaceFormats::isImage2dOr2dArray(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_TRUE(SurfaceFormats::isImage2dOr2dArray(CL_MEM_OBJECT_IMAGE2D_ARRAY));
    EXPECT_FALSE(SurfaceFormats::isImage2dOr2dArray(CL_MEM_OBJECT_IMAGE3D));
    EXPECT_FALSE(SurfaceFormats::isImage2dOr2dArray(CL_MEM_OBJECT_IMAGE1D_ARRAY));
}

TEST(SurfaceFormatsIsImage2dTests, givenImageTypesWhenCheckingIsImage3dThenOnlyImage3dMatches) {
    EXPECT_TRUE(SurfaceFormats::isImage3d(CL_MEM_OBJECT_IMAGE3D));
    EXPECT_FALSE(SurfaceFormats::isImage3d(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_FALSE(SurfaceFormats::isImage3d(CL_MEM_OBJECT_IMAGE2D_ARRAY));
}

TEST(SurfaceFormatsIsImage2dTests, givenImageTypesWhenCheckingIsImageArrayThenOnlyArrayTypesMatch) {
    EXPECT_TRUE(SurfaceFormats::isImageArray(CL_MEM_OBJECT_IMAGE1D_ARRAY));
    EXPECT_TRUE(SurfaceFormats::isImageArray(CL_MEM_OBJECT_IMAGE2D_ARRAY));
    EXPECT_FALSE(SurfaceFormats::isImageArray(CL_MEM_OBJECT_IMAGE1D));
    EXPECT_FALSE(SurfaceFormats::isImageArray(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_FALSE(SurfaceFormats::isImageArray(CL_MEM_OBJECT_IMAGE3D));
}

TEST(SurfaceFormatsIsDepthFormatTests, givenChannelOrdersWhenCheckingIsDepthFormatThenOnlyDepthOrdersMatch) {
    EXPECT_TRUE(SurfaceFormats::isDepthFormat(cl_image_format{CL_DEPTH, CL_FLOAT}));
    EXPECT_TRUE(SurfaceFormats::isDepthFormat(cl_image_format{CL_DEPTH_STENCIL, CL_UNORM_INT24}));
    EXPECT_FALSE(SurfaceFormats::isDepthFormat(cl_image_format{CL_R, CL_FLOAT}));
    EXPECT_FALSE(SurfaceFormats::isDepthFormat(cl_image_format{CL_RGBA, CL_UNORM_INT8}));
}

TEST(SurfaceFormatsGetImageDimensionsTests, givenImageTypesWhenGetImageHeightThenOnlyTwoAndThreeDimensionalTypesUseDescriptor) {
    cl_image_desc imageDesc{};
    imageDesc.image_height = 64;

    for (cl_mem_object_type imageType : {CL_MEM_OBJECT_IMAGE3D, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY}) {
        imageDesc.image_type = imageType;
        EXPECT_EQ(64u, SurfaceFormats::getImageHeight(imageDesc));
    }

    for (cl_mem_object_type imageType : {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY,
                                         CL_MEM_OBJECT_IMAGE1D_BUFFER, CL_MEM_OBJECT_BUFFER}) {
        imageDesc.image_type = imageType;
        EXPECT_EQ(1u, SurfaceFormats::getImageHeight(imageDesc));
    }
}

TEST(SurfaceFormatsGetImageDimensionsTests, givenImageTypesWhenGetImageDepthThenOnlyImage3dUsesDescriptor) {
    cl_image_desc imageDesc{};
    imageDesc.image_depth = 32;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    EXPECT_EQ(32u, SurfaceFormats::getImageDepth(imageDesc));

    for (cl_mem_object_type imageType : {CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE1D}) {
        imageDesc.image_type = imageType;
        EXPECT_EQ(1u, SurfaceFormats::getImageDepth(imageDesc));
    }
}

TEST(SurfaceFormatsGetImageDimensionsTests, givenImageTypesWhenGetImageArraySizeThenOnlyArrayTypesUseDescriptor) {
    cl_image_desc imageDesc{};
    imageDesc.image_array_size = 7;

    for (cl_mem_object_type imageType : {CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D_ARRAY}) {
        imageDesc.image_type = imageType;
        EXPECT_EQ(7u, SurfaceFormats::getImageArraySize(imageDesc));
    }

    for (cl_mem_object_type imageType : {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE3D}) {
        imageDesc.image_type = imageType;
        EXPECT_EQ(1u, SurfaceFormats::getImageArraySize(imageDesc));
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
