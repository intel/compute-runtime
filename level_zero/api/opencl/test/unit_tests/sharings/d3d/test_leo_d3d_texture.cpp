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
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/os_interface/windows/leo_d3d_sharing_functions.h"
#include "level_zero/api/opencl/source/sharings/d3d/leo_d3d_texture.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <memory>
#include <utility>
#include <vector>

namespace NEO {
namespace LEO {
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

TEST(D3DTextureFormatConversionTest, givenPackedYuvFormatWhenConvertingThenSupportedClAndL0FormatIsReturned) {
    struct {
        DXGI_FORMAT dxgiFormat;
        cl_channel_order channelOrder;
        cl_channel_type channelType;
        ze_image_format_layout_t l0Layout;
    } testCases[] = {
        {DXGI_FORMAT_YUY2, CL_YUYV_INTEL, CL_UNORM_INT8, ZE_IMAGE_FORMAT_LAYOUT_YUYV},
        {DXGI_FORMAT_AYUV, CL_RGBA, CL_UNORM_INT8, ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8},
        {DXGI_FORMAT_Y210, CL_RGBA, CL_UNORM_INT16, ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16},
        {DXGI_FORMAT_Y216, CL_RGBA, CL_UNORM_INT16, ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16},
        {DXGI_FORMAT_Y416, CL_RGBA, CL_UNORM_INT16, ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16},
        {DXGI_FORMAT_Y410, CL_RGBA, CL_UNORM_INT_101010_2, ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2},
    };

    for (const auto &testCase : testCases) {
        auto format = dxgiToOpenCLImageFormat(testCase.dxgiFormat, ImagePlane::noPlane);
        EXPECT_EQ(testCase.channelOrder, format.first);
        EXPECT_EQ(testCase.channelType, format.second);

        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, format.first, format.second);
        EXPECT_EQ(testCase.l0Layout, l0Format.layout);
        if (testCase.l0Layout != ZE_IMAGE_FORMAT_LAYOUT_YUYV) {
            EXPECT_EQ(ZE_IMAGE_FORMAT_TYPE_UNORM, l0Format.type);
        }
    }
}

TEST(D3DTextureFormatConversionTest, givenUnsupportedFormatWhenConvertingThenZeroFormatIsReturned) {
    auto format = dxgiToOpenCLImageFormat(DXGI_FORMAT_UNKNOWN, ImagePlane::noPlane);
    EXPECT_EQ(static_cast<cl_channel_order>(0), format.first);
    EXPECT_EQ(static_cast<cl_channel_type>(0), format.second);
}

class MockD3D11TextureSharingFunctions : public D3DSharingFunctions<D3DTypesHelper::D3D11> {
  public:
    MockD3D11TextureSharingFunctions() : D3DSharingFunctions<D3DTypesHelper::D3D11>(reinterpret_cast<ID3D11Device *>(0x1)) {}

    void getTexture2dDesc(D3D11_TEXTURE2D_DESC *textureDesc, ID3D11Texture2D *texture) override {
        *textureDesc = texture2dDescToReturn;
    }

    void getTexture3dDesc(D3D11_TEXTURE3D_DESC *textureDesc, ID3D11Texture3D *texture) override {
        *textureDesc = texture3dDescToReturn;
    }

    void createTexture2d(ID3D11Texture2D **texture, D3D11_TEXTURE2D_DESC *desc, cl_uint subresource) override {
        createTexture2dCalled++;
        *texture = reinterpret_cast<ID3D11Texture2D *>(0x200);
    }

    void createTexture3d(ID3D11Texture3D **texture, D3D11_TEXTURE3D_DESC *desc, cl_uint subresource) override {
        createTexture3dCalled++;
        *texture = reinterpret_cast<ID3D11Texture3D *>(0x300);
    }

    void getSharedHandle(ID3D11Resource *resource, void **handle) override {
        *handle = reinterpret_cast<void *>(0x400);
    }

    cl_int validateFormatSupport(DXGI_FORMAT format, cl_mem_object_type type) override {
        return CL_SUCCESS;
    }

    D3D11_TEXTURE2D_DESC texture2dDescToReturn = {};
    D3D11_TEXTURE3D_DESC texture3dDescToReturn = {};
    uint32_t createTexture2dCalled = 0u;
    uint32_t createTexture3dCalled = 0u;
};

struct ImageDescCapturingContext : public CapturingContext {
    using CapturingContext::CapturingContext;

    ze_result_t createImage(ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage) override {
        capturedImageDescs.push_back(*desc);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::vector<ze_image_desc_t> capturedImageDescs;
};

struct D3D11TextureCreateTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        cl_device_id clDeviceId = platform->getDevices()[0].get();
        capturingContext = std::make_unique<ImageDescCapturingContext>(driverHandle.get(), platform->getDevices()[0]->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
        sharingFcns = new MockD3D11TextureSharingFunctions();
        leoContext->registerSharing(sharingFcns);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    ID3D11Texture2D *d3dTexture2d = reinterpret_cast<ID3D11Texture2D *>(0x100);
    ID3D11Texture3D *d3dTexture3d = reinterpret_cast<ID3D11Texture3D *>(0x100);
    std::unique_ptr<ImageDescCapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
    MockD3D11TextureSharingFunctions *sharingFcns = nullptr;
};

TEST_F(D3D11TextureCreateTest, givenNonSharedTexture2dWithMipLevelsWhenCreatingImageFromMipSubresourceThenImageHasMipLevelDimensions) {
    sharingFcns->texture2dDescToReturn.Width = 256u;
    sharingFcns->texture2dDescToReturn.Height = 128u;
    sharingFcns->texture2dDescToReturn.MipLevels = 4u;
    sharingFcns->texture2dDescToReturn.ArraySize = 2u;
    sharingFcns->texture2dDescToReturn.Format = DXGI_FORMAT_R16_FLOAT;
    sharingFcns->texture2dDescToReturn.SampleDesc.Count = 1u;

    cl_int retCode = CL_SUCCESS;
    auto image = D3DTexture<D3DTypesHelper::D3D11>::create2d(leoContext.get(), d3dTexture2d, CL_MEM_READ_WRITE, 6u, &retCode);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(1u, sharingFcns->createTexture2dCalled);
    ASSERT_EQ(1u, capturingContext->capturedImageDescs.size());
    EXPECT_EQ(64u, capturingContext->capturedImageDescs[0].width);
    EXPECT_EQ(32u, capturingContext->capturedImageDescs[0].height);
}

TEST_F(D3D11TextureCreateTest, givenSharedTexture2dWithMipLevelsWhenCreatingImageFromFirstMipSubresourceThenImageHasTextureDimensions) {
    sharingFcns->texture2dDescToReturn.Width = 256u;
    sharingFcns->texture2dDescToReturn.Height = 128u;
    sharingFcns->texture2dDescToReturn.MipLevels = 4u;
    sharingFcns->texture2dDescToReturn.ArraySize = 1u;
    sharingFcns->texture2dDescToReturn.Format = DXGI_FORMAT_R16_FLOAT;
    sharingFcns->texture2dDescToReturn.SampleDesc.Count = 1u;
    sharingFcns->texture2dDescToReturn.MiscFlags = D3DResourceFlags::MISC_SHARED;

    cl_int retCode = CL_SUCCESS;
    auto image = D3DTexture<D3DTypesHelper::D3D11>::create2d(leoContext.get(), d3dTexture2d, CL_MEM_READ_WRITE, 0u, &retCode);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(0u, sharingFcns->createTexture2dCalled);
    ASSERT_EQ(1u, capturingContext->capturedImageDescs.size());
    EXPECT_EQ(256u, capturingContext->capturedImageDescs[0].width);
    EXPECT_EQ(128u, capturingContext->capturedImageDescs[0].height);
}

TEST_F(D3D11TextureCreateTest, givenTexture3dWithMipLevelsWhenCreatingImageFromMipSubresourceThenImageHasMipLevelDimensions) {
    sharingFcns->texture3dDescToReturn.Width = 64u;
    sharingFcns->texture3dDescToReturn.Height = 32u;
    sharingFcns->texture3dDescToReturn.Depth = 16u;
    sharingFcns->texture3dDescToReturn.MipLevels = 3u;
    sharingFcns->texture3dDescToReturn.Format = DXGI_FORMAT_R16_FLOAT;

    cl_int retCode = CL_SUCCESS;
    auto image = D3DTexture<D3DTypesHelper::D3D11>::create3d(leoContext.get(), d3dTexture3d, CL_MEM_READ_WRITE, 1u, &retCode);

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(1u, sharingFcns->createTexture3dCalled);
    ASSERT_EQ(1u, capturingContext->capturedImageDescs.size());
    EXPECT_EQ(32u, capturingContext->capturedImageDescs[0].width);
    EXPECT_EQ(16u, capturingContext->capturedImageDescs[0].height);
    EXPECT_EQ(8u, capturingContext->capturedImageDescs[0].depth);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
