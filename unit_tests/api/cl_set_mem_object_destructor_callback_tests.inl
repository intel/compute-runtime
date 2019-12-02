/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateBufferTests;

namespace ULT {

static int cbInvoked = 0;
void CL_CALLBACK destructorCallback(cl_mem memObj, void *userData) {
    cbInvoked++;
}

struct clSetMemObjectDestructorCallbackTests : public ApiFixture,
                                               public ::testing::Test {

    void SetUp() override {
        ApiFixture::SetUp();

        // clang-format off
        imageFormat.image_channel_order     = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = 32;
        imageDesc.image_height      = 32;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object        = nullptr;
        // clang-format on

        cbInvoked = 0;
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

TEST_F(clSetMemObjectDestructorCallbackTests, GivenNullMemObjWhenSettingMemObjCallbackThenInvalidMemObjectErrorIsReturned) {
    retVal = clSetMemObjectDestructorCallback(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
    EXPECT_EQ(0, cbInvoked);
}

TEST_F(clSetMemObjectDestructorCallbackTests, GivenImageAndDestructorCallbackWhenSettingMemObjCallbackThenSuccessIsReturned) {
    auto image = clCreateImage(pContext, CL_MEM_READ_WRITE, &imageFormat,
                               &imageDesc, nullptr, &retVal);

    retVal = clSetMemObjectDestructorCallback(image, destructorCallback, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, cbInvoked);
}

TEST_F(clSetMemObjectDestructorCallbackTests, GivenImageAndNullCallbackFunctionWhenSettingMemObjCallbackThenInvalidValueErrorIsReturned) {
    auto image = clCreateImage(pContext, CL_MEM_READ_WRITE, &imageFormat,
                               &imageDesc, nullptr, &retVal);

    retVal = clSetMemObjectDestructorCallback(image, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, cbInvoked);
}

TEST_F(clSetMemObjectDestructorCallbackTests, GivenBufferAndDestructorCallbackFunctionWhenSettingMemObjCallbackThenSuccessIsReturned) {
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 42, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clSetMemObjectDestructorCallback(buffer, destructorCallback, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, cbInvoked);
}

TEST_F(clSetMemObjectDestructorCallbackTests, GivenBufferAndNullCallbackFunctionWhenSettingMemObjCallbackThenInvalidValueErrorIsReturned) {
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 42, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cbInvoked = 0;
    retVal = clSetMemObjectDestructorCallback(buffer, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, cbInvoked);
}
} // namespace ULT
