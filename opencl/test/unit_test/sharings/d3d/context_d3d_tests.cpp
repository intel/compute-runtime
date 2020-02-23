/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/os_interface/windows/d3d_sharing_functions.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(D3DContextTest, sharingAreNotPresentByDefault) {
    MockContext context;

    EXPECT_EQ(nullptr, context.getSharing<D3DSharingFunctions<D3DTypesHelper::D3D9>>());
    EXPECT_EQ(nullptr, context.getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>());
    EXPECT_EQ(nullptr, context.getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>());
}

TEST(D3DContextTest, giveDispatchtableContainsValidEntries) {
    sharingFactory.fillGlobalDispatchTable();

    MockContext context;

    EXPECT_EQ(&clGetDeviceIDsFromDX9INTEL, context.dispatch.crtDispatch->clGetDeviceIDsFromDX9INTEL);
    EXPECT_EQ(&clCreateFromDX9MediaSurfaceINTEL, context.dispatch.crtDispatch->clCreateFromDX9MediaSurfaceINTEL);
    EXPECT_EQ(&clEnqueueAcquireDX9ObjectsINTEL, context.dispatch.crtDispatch->clEnqueueAcquireDX9ObjectsINTEL);
    EXPECT_EQ(&clEnqueueReleaseDX9ObjectsINTEL, context.dispatch.crtDispatch->clEnqueueReleaseDX9ObjectsINTEL);
}

struct clIntelSharingFormatQueryDX9 : public api_tests {
    std::vector<D3DFORMAT> supportedNonPlanarFormats;
    std::vector<D3DFORMAT> supportedPlanarFormats;
    std::vector<D3DFORMAT> supportedPlane1Formats;
    std::vector<D3DFORMAT> supportedPlane2Formats;
    std::vector<D3DFORMAT> retrievedFormats;

    cl_uint numImageFormats;

    void SetUp() override {
        api_tests::SetUp();
        supportedNonPlanarFormats = {D3DFMT_R32F, D3DFMT_R16F, D3DFMT_L16, D3DFMT_A8,
                                     D3DFMT_L8, D3DFMT_G32R32F, D3DFMT_G16R16F, D3DFMT_G16R16,
                                     D3DFMT_A8L8, D3DFMT_A32B32G32R32F, D3DFMT_A16B16G16R16F, D3DFMT_A16B16G16R16,
                                     D3DFMT_A8B8G8R8, D3DFMT_X8B8G8R8, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8};

        supportedPlanarFormats = {D3DFMT_YUY2, D3DFMT_UYVY,
                                  static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')),
                                  static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2')),
                                  static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', 'Y', 'U')),
                                  static_cast<D3DFORMAT>(MAKEFOURCC('V', 'Y', 'U', 'Y'))};

        supportedPlane1Formats = {static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')),
                                  static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2'))};

        supportedPlane2Formats = {static_cast<D3DFORMAT>(MAKEFOURCC('Y', 'V', '1', '2'))};
        retrievedFormats.assign(supportedNonPlanarFormats.size() + supportedPlanarFormats.size(), D3DFMT_UNKNOWN);
    }

    void TearDown() override {
        api_tests::TearDown();
    }
};

namespace ULT {

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidContextWhenMediaSurfaceFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(nullptr, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                       static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0],
                                                       &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidFlagsWhenMediaSurfaceFormatsRequestedThenInvalidValueError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(
        pContext, 0, CL_MEM_OBJECT_IMAGE2D, 0,
        static_cast<cl_uint>(retrievedFormats.size()),
        &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidImageTypeWhenMediaSurfaceFormatsRequestedThenInvalidValueError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, 0, 0, static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9,
       givenValidParametersWhenRequestedMediaSurfaceFormatsBelowMaximumThenExceedingFormatAreaRemainsUntouched) {
    for (cl_uint i = 0; i <= static_cast<cl_uint>(retrievedFormats.size()); ++i) {
        retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, i,
                                                           &retrievedFormats[0], &numImageFormats);
        EXPECT_EQ(retVal, CL_SUCCESS);
        for (cl_uint j = i; j < retrievedFormats.size(); ++j) {
            EXPECT_EQ(retrievedFormats[j], D3DFMT_UNKNOWN);
        }
    }
}

TEST_F(clIntelSharingFormatQueryDX9, givenValidParametersWhenRequestingMediaSurfaceFormatsForPlane0ThenAllKnownFormatsAreIncludedInTheResult) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0,
                                                       static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0],
                                                       &numImageFormats);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(supportedNonPlanarFormats.size() + supportedPlanarFormats.size(), numImageFormats);

    for (auto format : supportedNonPlanarFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }

    for (auto format : supportedPlanarFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX9, givenValidParametersWhenRequestingMediaSurfaceFormatsForPlane1ThenOnlyPlanarFormatsAreIncludedInTheResult) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 1,
                                                       static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0],
                                                       &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(supportedPlane1Formats.size(), numImageFormats);

    for (auto format : supportedPlane1Formats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }

    for (auto format : supportedNonPlanarFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_EQ(found, retrievedFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX9, givenValidParametersWhenRequestingMediaSurfaceFormatsForPlane2ThenOnlyYV12FormatIsIncludedInTheResult) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 2,
                                                       static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0],
                                                       &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(supportedPlane2Formats.size(), numImageFormats);

    for (auto format : supportedPlane2Formats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }

    for (auto format : supportedNonPlanarFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_EQ(found, retrievedFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX9, givenValidParametersWhenRequestingMediaSurfaceFormatsForPlaneGraterThan2ThenZeroNumFormatsIsReturned) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 3,
                                                       0,
                                                       nullptr,
                                                       &numImageFormats);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(0u, numImageFormats);
}
} // namespace ULT
