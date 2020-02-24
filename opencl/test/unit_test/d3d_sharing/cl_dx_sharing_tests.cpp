/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/arrayref.h"

#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/d3d/d3d_buffer.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/source/sharings/d3d/d3d_texture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_d3d_objects.h"
#include "test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

static const DXGI_FORMAT DXGIformats[] = {
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB,
    DXGI_FORMAT_AYUV,
    DXGI_FORMAT_Y410,
    DXGI_FORMAT_Y416,
    DXGI_FORMAT_NV12,
    DXGI_FORMAT_P010,
    DXGI_FORMAT_P016,
    DXGI_FORMAT_420_OPAQUE,
    DXGI_FORMAT_YUY2,
    DXGI_FORMAT_Y210,
    DXGI_FORMAT_Y216,
    DXGI_FORMAT_NV11,
    DXGI_FORMAT_AI44,
    DXGI_FORMAT_IA44,
    DXGI_FORMAT_P8,
    DXGI_FORMAT_A8P8,
    DXGI_FORMAT_B4G4R4A4_UNORM,
    DXGI_FORMAT_P208,
    DXGI_FORMAT_V208,
    DXGI_FORMAT_V408,
    DXGI_FORMAT_FORCE_UINT};

template <typename T>
struct clIntelSharingFormatQueryDX1X : public PlatformFixture, public ::testing::Test {
    std::vector<DXGI_FORMAT> retrievedFormats;
    ArrayRef<const DXGI_FORMAT> availableFormats;

    NiceMock<MockD3DSharingFunctions<T>> *mockSharingFcns;
    MockContext *context;
    cl_uint numImageFormats;
    cl_int retVal;
    size_t retSize;

    void SetUp() override {
        PlatformFixture::SetUp();
        context = new MockContext(pPlatform->getClDevice(0));
        mockSharingFcns = new NiceMock<MockD3DSharingFunctions<T>>();
        context->setSharingFunctions(mockSharingFcns);

        availableFormats = ArrayRef<const DXGI_FORMAT>(DXGIformats);
        retrievedFormats.assign(availableFormats.size(), DXGI_FORMAT_UNKNOWN);
    }
    void TearDown() override {
        delete context;
        PlatformFixture::TearDown();
    }
};

typedef clIntelSharingFormatQueryDX1X<D3DTypesHelper::D3D10> clIntelSharingFormatQueryDX10;
typedef clIntelSharingFormatQueryDX1X<D3DTypesHelper::D3D11> clIntelSharingFormatQueryDX11;

TEST_F(clIntelSharingFormatQueryDX10, givenInvalidContextWhenDX10TextureFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedD3D10TextureFormatsINTEL(NULL, CL_MEM_READ_WRITE, 0,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX10, givenValidParametersWhenRequestedDX10TextureFormatsThenTheResultIsASubsetOfKnownFormats) {
    retVal = clGetSupportedD3D10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    for (cl_uint i = 0; i < numImageFormats; ++i) {
        EXPECT_NE(std::find(availableFormats.begin(),
                            availableFormats.end(),
                            retrievedFormats[i]),
                  availableFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX10, givenValidParametersWhenRequestedDX10TextureFormatsTwiceThenTheResultsAreTheSame) {
    retVal = clGetSupportedD3D10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    std::vector<DXGI_FORMAT> formatsRetrievedForTheSecondTime(availableFormats.size());
    cl_uint anotherNumImageFormats;
    retVal = clGetSupportedD3D10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                    static_cast<cl_uint>(formatsRetrievedForTheSecondTime.size()),
                                                    &formatsRetrievedForTheSecondTime[0], &anotherNumImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_EQ(numImageFormats, anotherNumImageFormats);
    ASSERT_EQ(memcmp(&retrievedFormats[0], &formatsRetrievedForTheSecondTime[0], numImageFormats * sizeof(DXGI_FORMAT)), 0);
}

TEST_F(clIntelSharingFormatQueryDX10, givenNullFormatsWhenRequestedDX10TextureFormatsThenNumImageFormatsIsSane) {
    retVal = clGetSupportedD3D10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_LE(0U, numImageFormats);
    ASSERT_LE(numImageFormats, static_cast<cl_uint>(availableFormats.size()));
}

TEST_F(clIntelSharingFormatQueryDX10, givenNullPointersWhenRequestedDX10TextureFormatsThenCLSuccessIsReturned) {
    retVal = clGetSupportedD3D10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, static_cast<cl_uint>(retrievedFormats.size()), nullptr, nullptr);
    ASSERT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clIntelSharingFormatQueryDX11, givenInvalidContextWhenDX11TextureFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedD3D11TextureFormatsINTEL(nullptr, CL_MEM_READ_WRITE, 0, 0,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX11, givenValidParametersWhenRequestedDX11TextureFormatsThenTheResultIsASubsetOfKnownFormats) {
    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    for (cl_uint i = 0; i < numImageFormats; ++i) {
        EXPECT_NE(std::find(availableFormats.begin(),
                            availableFormats.end(),
                            retrievedFormats[i]),
                  availableFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX11, givenNullFormatsWhenRequestedDX11TextureFormatsThenNumImageFormatsIsSane) {
    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, 0, nullptr, &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_LE(0U, numImageFormats);
    ASSERT_LE(numImageFormats, static_cast<cl_uint>(availableFormats.size()));
}

TEST_F(clIntelSharingFormatQueryDX11, givenNullPointersWhenRequestedDX11TextureFormatsThenCLSuccessIsReturned) {
    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                    static_cast<cl_uint>(retrievedFormats.size()), nullptr, nullptr);
    ASSERT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clIntelSharingFormatQueryDX11, givenValidParametersWhenRequestedDX11TextureFormatsTwiceThenTheResultsAreTheSame) {
    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    std::vector<DXGI_FORMAT> formatsRetrievedForTheSecondTime(availableFormats.size());
    cl_uint anotherNumImageFormats;
    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                    static_cast<cl_uint>(formatsRetrievedForTheSecondTime.size()),
                                                    &formatsRetrievedForTheSecondTime[0], &anotherNumImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_EQ(numImageFormats, anotherNumImageFormats);
    ASSERT_EQ(memcmp(&retrievedFormats[0], &formatsRetrievedForTheSecondTime[0], numImageFormats * sizeof(DXGI_FORMAT)), 0);
}

TEST_F(clIntelSharingFormatQueryDX11, givenValidParametersWhenRequestingDX11TextureFormatsForPlane1ThenPlanarFormatsAeReturned) {

    auto checkFormat = [](DXGI_FORMAT format, UINT *pFormat) -> void { *pFormat = D3D11_FORMAT_SUPPORT_TEXTURE2D; };

    ON_CALL(*mockSharingFcns, checkFormatSupport(::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(checkFormat));
    ON_CALL(*mockSharingFcns, memObjectFormatSupport(::testing::_, ::testing::_)).WillByDefault(::testing::Return(true));

    retVal = clGetSupportedD3D11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 1,
                                                    static_cast<cl_uint>(retrievedFormats.size()),
                                                    &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(retVal, CL_SUCCESS);

    std::vector<DXGI_FORMAT> expectedPlanarFormats = {DXGI_FORMAT_NV12, DXGI_FORMAT_P010, DXGI_FORMAT_P016};
    EXPECT_EQ(expectedPlanarFormats.size(), numImageFormats);

    for (auto format : expectedPlanarFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }
}
