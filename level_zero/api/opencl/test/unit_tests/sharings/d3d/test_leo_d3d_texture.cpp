/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/d3d/leo_d3d_texture.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <utility>

namespace NEO {
namespace LEO {

std::pair<cl_channel_order, cl_channel_type> dxgiToOpenCLImageFormat(DXGI_FORMAT dxgiFormat, ImagePlane plane);

namespace ult {

TEST(D3DTextureFormatConversionTest, givenNV12FormatWhenConvertingThenChannelOrderFollowsPlaneAndTypeIsUnormInt8) {
    auto planeY = dxgiToOpenCLImageFormat(DXGI_FORMAT_NV12, ImagePlane::planeY);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), planeY.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), planeY.second);

    auto planeUV = dxgiToOpenCLImageFormat(DXGI_FORMAT_NV12, ImagePlane::planeUV);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), planeUV.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), planeUV.second);

    auto noPlane = dxgiToOpenCLImageFormat(DXGI_FORMAT_NV12, ImagePlane::noPlane);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), noPlane.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), noPlane.second);
}

TEST(D3DTextureFormatConversionTest, givenP010FormatWhenConvertingThenChannelOrderFollowsPlaneAndTypeIsUnormInt16) {
    auto planeY = dxgiToOpenCLImageFormat(DXGI_FORMAT_P010, ImagePlane::planeY);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), planeY.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), planeY.second);

    auto planeUV = dxgiToOpenCLImageFormat(DXGI_FORMAT_P010, ImagePlane::planeUV);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), planeUV.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), planeUV.second);

    auto noPlane = dxgiToOpenCLImageFormat(DXGI_FORMAT_P010, ImagePlane::noPlane);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), noPlane.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), noPlane.second);
}

TEST(D3DTextureFormatConversionTest, givenP016FormatWhenConvertingThenChannelOrderFollowsPlaneAndTypeIsUnormInt16) {
    auto planeY = dxgiToOpenCLImageFormat(DXGI_FORMAT_P016, ImagePlane::planeY);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), planeY.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), planeY.second);

    auto planeUV = dxgiToOpenCLImageFormat(DXGI_FORMAT_P016, ImagePlane::planeUV);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), planeUV.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), planeUV.second);

    auto noPlane = dxgiToOpenCLImageFormat(DXGI_FORMAT_P016, ImagePlane::noPlane);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), noPlane.first);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), noPlane.second);
}

TEST(D3DTextureFormatConversionTest, givenNonPlanarFormatWhenConvertingThenPlaneArgumentIsIgnored) {
    for (auto plane : {ImagePlane::planeY, ImagePlane::planeUV, ImagePlane::noPlane}) {
        auto singleChannel = dxgiToOpenCLImageFormat(DXGI_FORMAT_R8_UNORM, plane);
        EXPECT_EQ(static_cast<cl_channel_order>(CL_R), singleChannel.first);
        EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), singleChannel.second);

        auto fourChannel = dxgiToOpenCLImageFormat(DXGI_FORMAT_R8G8B8A8_UNORM, plane);
        EXPECT_EQ(static_cast<cl_channel_order>(CL_RGBA), fourChannel.first);
        EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), fourChannel.second);
    }
}

TEST(D3DTextureFormatConversionTest, givenUnsupportedFormatWhenConvertingThenZeroFormatIsReturned) {
    auto format = dxgiToOpenCLImageFormat(DXGI_FORMAT_UNKNOWN, ImagePlane::noPlane);
    EXPECT_EQ(static_cast<cl_channel_order>(0), format.first);
    EXPECT_EQ(static_cast<cl_channel_type>(0), format.second);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
