/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/d3d/cl_d3d_api.h"
#include "runtime/os_interface/windows/d3d_sharing_functions.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/helpers/gtest_helpers.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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
    std::vector<D3DFORMAT> supportedFormats;
    std::vector<D3DFORMAT> retrievedFormats;
    cl_uint numImageFormats;

    void SetUp() override {
        api_tests::SetUp();
        supportedFormats = {D3DFMT_R32F, D3DFMT_R16F, D3DFMT_L16, D3DFMT_A8,
                            D3DFMT_L8, D3DFMT_G32R32F, D3DFMT_G16R16F, D3DFMT_G16R16,
                            D3DFMT_A8L8, D3DFMT_A32B32G32R32F, D3DFMT_A16B16G16R16F, D3DFMT_A16B16G16R16,
                            D3DFMT_A8B8G8R8, D3DFMT_X8B8G8R8, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8};
        retrievedFormats.assign(supportedFormats.size(), D3DFMT_UNKNOWN);
    }

    void TearDown() override { api_tests::TearDown(); }
};

namespace ULT {

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidContextWhenMediaSurfaceFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(nullptr, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                       static_cast<cl_uint>(retrievedFormats.size()), &retrievedFormats[0],
                                                       &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidFlagsWhenMediaSurfaceFormatsRequestedThenInvalidValueError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(
        pContext, 0, CL_MEM_OBJECT_IMAGE2D, static_cast<cl_uint>(retrievedFormats.size()), &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9, givenInvalidImageTypeWhenMediaSurfaceFormatsRequestedThenInvalidValueError) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, 0, static_cast<cl_uint>(retrievedFormats.size()),
                                                       &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clIntelSharingFormatQueryDX9,
       givenValidParametersWhenRequestedMediaSurfaceFormatsBelowMaximumThenExceedingFormatAreaRemainsUntouched) {
    for (cl_uint i = 0; i <= static_cast<cl_uint>(retrievedFormats.size()); ++i) {
        retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, i,
                                                           &retrievedFormats[0], &numImageFormats);
        EXPECT_EQ(retVal, CL_SUCCESS);
        for (cl_uint j = i; j < retrievedFormats.size(); ++j) {
            EXPECT_EQ(retrievedFormats[j], D3DFMT_UNKNOWN);
        }
    }
}

TEST_F(clIntelSharingFormatQueryDX9, givenValidParametersWhenRequestedMediaSurfaceFormatsThenAllKnownFormatsAreIncludedInTheResult) {
    retVal = clGetSupportedDX9MediaSurfaceFormatsINTEL(pContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                       static_cast<cl_uint>(retrievedFormats.size()), &retrievedFormats[0],
                                                       &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);

    EXPECT_LE(supportedFormats.size(), numImageFormats);

    for (auto format : supportedFormats) {
        auto found = std::find(retrievedFormats.begin(), retrievedFormats.end(), format);
        EXPECT_NE(found, retrievedFormats.end());
    }
}
} // namespace ULT
