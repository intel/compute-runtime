/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_convert_color.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <cmath>
#include <limits>

namespace NEO {
namespace LEO {
namespace ult {

TEST(SelectNormalizingFactorTests, givenUnormInt8WhenSelectNormalizingFactorThenReturns255) {
    EXPECT_EQ(0xFF, selectNormalizingFactor(CL_UNORM_INT8));
}

TEST(SelectNormalizingFactorTests, givenSnormInt8WhenSelectNormalizingFactorThenReturns127) {
    EXPECT_EQ(0x7F, selectNormalizingFactor(CL_SNORM_INT8));
}

TEST(SelectNormalizingFactorTests, givenUnormInt16WhenSelectNormalizingFactorThenReturns65535) {
    EXPECT_EQ(0xFFFF, selectNormalizingFactor(CL_UNORM_INT16));
}

TEST(SelectNormalizingFactorTests, givenSnormInt16WhenSelectNormalizingFactorThenReturns32767) {
    EXPECT_EQ(0x7FFF, selectNormalizingFactor(CL_SNORM_INT16));
}

TEST(SelectNormalizingFactorTests, givenNonNormalizedTypeWhenSelectNormalizingFactorThenReturnsZero) {
    EXPECT_EQ(0, selectNormalizingFactor(CL_FLOAT));
    EXPECT_EQ(0, selectNormalizingFactor(CL_HALF_FLOAT));
    EXPECT_EQ(0, selectNormalizingFactor(CL_SIGNED_INT8));
    EXPECT_EQ(0, selectNormalizingFactor(CL_UNSIGNED_INT8));
    EXPECT_EQ(0, selectNormalizingFactor(CL_UNSIGNED_INT32));
}

TEST(ConvertFillColorTests, givenUnchangedFormatWhenConvertFillColorThenIntegerChannelsArePassedThrough) {
    const int32_t sourceColor[4] = {11, 22, 33, 44};
    int32_t destinationColor[4] = {};
    cl_image_format format{CL_RGBA, CL_UNSIGNED_INT32};

    convertFillColor(sourceColor, destinationColor, format, format);

    EXPECT_EQ(11, destinationColor[0]);
    EXPECT_EQ(22, destinationColor[1]);
    EXPECT_EQ(33, destinationColor[2]);
    EXPECT_EQ(44, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenChannelOrderAWhenConvertFillColorThenFirstAndLastChannelsAreSwapped) {
    const int32_t sourceColor[4] = {1, 2, 3, 4};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_A, CL_UNSIGNED_INT32};
    cl_image_format newFormat{CL_R, CL_UNSIGNED_INT32};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(4, destinationColor[0]);
    EXPECT_EQ(2, destinationColor[1]);
    EXPECT_EQ(3, destinationColor[2]);
    EXPECT_EQ(1, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenChannelOrderBgraWhenConvertFillColorThenRedAndBlueChannelsAreSwapped) {
    const int32_t sourceColor[4] = {1, 2, 3, 4};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_BGRA, CL_UNSIGNED_INT32};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT32};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(3, destinationColor[0]);
    EXPECT_EQ(2, destinationColor[1]);
    EXPECT_EQ(1, destinationColor[2]);
    EXPECT_EQ(4, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenChannelOrderSbgraWhenConvertFillColorThenRedAndBlueChannelsAreSwapped) {
    const int32_t sourceColor[4] = {1, 2, 3, 4};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sBGRA, CL_UNSIGNED_INT32};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT32};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(3, destinationColor[0]);
    EXPECT_EQ(1, destinationColor[2]);
}

TEST(ConvertFillColorTests, givenChannelOrderRgbaWhenConvertFillColorThenNoChannelsAreSwapped) {
    const int32_t sourceColor[4] = {1, 2, 3, 4};
    int32_t destinationColor[4] = {};
    cl_image_format format{CL_RGBA, CL_UNSIGNED_INT32};

    convertFillColor(sourceColor, destinationColor, format, format);

    EXPECT_EQ(1, destinationColor[0]);
    EXPECT_EQ(3, destinationColor[2]);
    EXPECT_EQ(4, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenUnsignedInt8TargetWithoutNormalizingFactorWhenConvertFillColorThenChannelsAreOnlyMasked) {
    const int32_t sourceColor[4] = {0x1FF, 0x2AB, -1, 0x100};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_UNSIGNED_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0xFF, destinationColor[0]);
    EXPECT_EQ(0xAB, destinationColor[1]);
    EXPECT_EQ(0xFF, destinationColor[2]);
    EXPECT_EQ(0x00, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenUnormInt8SourceAndUnsignedInt8TargetWhenConvertFillColorThenFloatChannelsAreDenormalized) {
    const float sourceColor[4] = {1.0f, 0.5f, 0.0f, 0.25f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(255, destinationColor[0]);
    EXPECT_EQ(127, destinationColor[1]);
    EXPECT_EQ(0, destinationColor[2]);
    EXPECT_EQ(63, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenSnormInt8SourceAndUnsignedInt8TargetWhenConvertFillColorThenNegativeChannelsWrapAfterMasking) {
    const float sourceColor[4] = {-1.0f, 1.0f, 0.0f, 0.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_SNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0x81, destinationColor[0]);
    EXPECT_EQ(0x7F, destinationColor[1]);
}

TEST(ConvertFillColorTests, givenUnormInt16SourceAndUnsignedInt16TargetWhenConvertFillColorThenFloatChannelsAreDenormalized) {
    const float sourceColor[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_UNORM_INT16};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT16};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0xFFFF, destinationColor[0]);
    EXPECT_EQ(0x7FFF, destinationColor[1]);
    EXPECT_EQ(0, destinationColor[2]);
    EXPECT_EQ(0xFFFF, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenHalfFloatSourceAndUnsignedInt16TargetWhenConvertFillColorThenChannelsAreConvertedToHalf) {
    const float sourceColor[4] = {1.0f, 0.5f, 0.0f, 2.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_HALF_FLOAT};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT16};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    for (auto i = 0; i < 4; i++) {
        EXPECT_EQ(static_cast<int32_t>(Math::float2Half(sourceColor[i])), destinationColor[i]);
    }
}

TEST(ConvertFillColorTests, givenUnsignedInt16TargetWithoutNormalizingFactorOrHalfFloatWhenConvertFillColorThenChannelsAreOnlyMasked) {
    const int32_t sourceColor[4] = {0x1FFFF, 0x2ABCD, -1, 0x10000};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_RGBA, CL_UNSIGNED_INT16};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT16};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0xFFFF, destinationColor[0]);
    EXPECT_EQ(0xABCD, destinationColor[1]);
    EXPECT_EQ(0xFFFF, destinationColor[2]);
    EXPECT_EQ(0x0000, destinationColor[3]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceWithLinearChannelsWhenConvertFillColorThenGammaIsAppliedToColorChannelsOnly) {
    const float sourceColor[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    const float encoded = 1.055f * std::pow(0.25f, 1.0f / 2.4f) - 0.055f;
    const auto expectedEncoded = static_cast<int32_t>(0xFF * encoded + 0.5f);
    EXPECT_EQ(expectedEncoded, destinationColor[0]);
    EXPECT_EQ(expectedEncoded, destinationColor[1]);
    EXPECT_EQ(expectedEncoded, destinationColor[2]);
    EXPECT_EQ(static_cast<int32_t>(0xFF * 0.25f), destinationColor[3]);
    EXPECT_NE(destinationColor[0], destinationColor[3]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceWithSmallChannelsWhenConvertFillColorThenLinearSegmentIsApplied) {
    const float sourceColor[4] = {0.001f, 0.001f, 0.001f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    const float encoded = 12.92f * 0.001f;
    const auto expectedEncoded = static_cast<int32_t>(0xFF * encoded + 0.5f);
    EXPECT_EQ(expectedEncoded, destinationColor[0]);
    EXPECT_EQ(expectedEncoded, destinationColor[2]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceWithChannelAboveOneWhenConvertFillColorThenChannelIsClampedToMax) {
    const float sourceColor[4] = {2.0f, 2.0f, 2.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0xFF, destinationColor[0]);
    EXPECT_EQ(0xFF, destinationColor[1]);
    EXPECT_EQ(0xFF, destinationColor[2]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceWithNegativeChannelWhenConvertFillColorThenChannelIsClampedToZero) {
    const float sourceColor[4] = {-1.0f, -0.5f, -2.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0, destinationColor[0]);
    EXPECT_EQ(0, destinationColor[1]);
    EXPECT_EQ(0, destinationColor[2]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceWithNanChannelWhenConvertFillColorThenChannelIsTreatedAsZero) {
    const float sourceColor[4] = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0, destinationColor[0]);
}

TEST(ConvertFillColorTests, givenSbgraSourceWhenConvertFillColorThenChannelsAreSwappedBeforeGammaEncoding) {
    const float sourceColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sBGRA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT8};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    EXPECT_EQ(0xFF, destinationColor[0]);
    EXPECT_EQ(0, destinationColor[1]);
    EXPECT_EQ(0, destinationColor[2]);
}

TEST(ConvertFillColorTests, givenSrgbaSourceAndUnsignedInt16TargetWhenConvertFillColorThenNoRoundingOffsetIsApplied) {
    const float sourceColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    int32_t destinationColor[4] = {};
    cl_image_format oldFormat{CL_sRGBA, CL_UNORM_INT8};
    cl_image_format newFormat{CL_RGBA, CL_UNSIGNED_INT16};

    convertFillColor(sourceColor, destinationColor, oldFormat, newFormat);

    const float encoded = 1.055f * std::pow(1.0f, 1.0f / 2.4f) - 0.055f;
    EXPECT_EQ(static_cast<int32_t>(0xFF * encoded), destinationColor[0]);
    EXPECT_EQ(static_cast<int32_t>(0xFF * 1.0f), destinationColor[3]);
}

TEST(RedescribeFillColorTests, givenSingleChannelSingleByteFormatWhenRedescribeFillColorThenReturnsRWithUnsignedInt8) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.numChannels = 1;
    surfaceFormat.perChannelSizeInBytes = 1;
    ImageInfo imageInfo{};
    imageInfo.surfaceFormat = &surfaceFormat;

    auto format = redescribeFillColor(imageInfo);

    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), format.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNSIGNED_INT8), format.image_channel_data_type);
}

TEST(RedescribeFillColorTests, givenTwoChannelTwoByteFormatWhenRedescribeFillColorThenReturnsRgWithUnsignedInt16) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.numChannels = 2;
    surfaceFormat.perChannelSizeInBytes = 2;
    ImageInfo imageInfo{};
    imageInfo.surfaceFormat = &surfaceFormat;

    auto format = redescribeFillColor(imageInfo);

    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), format.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNSIGNED_INT16), format.image_channel_data_type);
}

TEST(RedescribeFillColorTests, givenFourChannelFourByteFormatWhenRedescribeFillColorThenReturnsRgbaWithUnsignedInt32) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.numChannels = 4;
    surfaceFormat.perChannelSizeInBytes = 4;
    ImageInfo imageInfo{};
    imageInfo.surfaceFormat = &surfaceFormat;

    auto format = redescribeFillColor(imageInfo);

    EXPECT_EQ(static_cast<cl_channel_order>(CL_RGBA), format.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNSIGNED_INT32), format.image_channel_data_type);
}

TEST(RedescribeFillColorTests, givenThreeChannelFormatWhenRedescribeFillColorThenReturnsRgba) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.numChannels = 3;
    surfaceFormat.perChannelSizeInBytes = 8;
    ImageInfo imageInfo{};
    imageInfo.surfaceFormat = &surfaceFormat;

    auto format = redescribeFillColor(imageInfo);

    EXPECT_EQ(static_cast<cl_channel_order>(CL_RGBA), format.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNSIGNED_INT32), format.image_channel_data_type);
}

TEST(RedescribeFillColorTests, givenZeroChannelFormatWhenRedescribeFillColorThenReturnsR) {
    SurfaceFormatInfo surfaceFormat{};
    surfaceFormat.numChannels = 0;
    surfaceFormat.perChannelSizeInBytes = 0;
    ImageInfo imageInfo{};
    imageInfo.surfaceFormat = &surfaceFormat;

    auto format = redescribeFillColor(imageInfo);

    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), format.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNSIGNED_INT8), format.image_channel_data_type);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
