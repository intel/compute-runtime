/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/accelerators/intel_motion_estimation.h"
#include "runtime/context/context.h"
#include "unit_tests/api/cl_api_tests.h"

using namespace OCLRT;

namespace ULT {

struct IntelMotionEstimationTest : public api_tests {
  public:
    IntelMotionEstimationTest() {}

    void SetUp() override {
        api_tests::SetUp();

        desc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
        desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
        desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_HAAR_INTEL;
        desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;
    }

    void TearDown() override {
        api_tests::TearDown();
    }

  protected:
    cl_accelerator_intel accelerator = nullptr;
    cl_motion_estimation_desc_intel desc;
    cl_int retVal = 0xEEEEEEEEu;
    cl_int result = -1;
};

typedef IntelMotionEstimationTest IntelMotionEstimationNegativeTest;

TEST_F(IntelMotionEstimationNegativeTest, createWithNullDescriptorExpectFail) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenDescriptorSizeLongExpectFail) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel) + 1,
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenDescriptorSizeShortExpectFail) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel) - 1,
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenInvalidMacroBlockType) {
    desc.mb_block_type = 0xEEEEEEEE;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenInvalidSubPixelMode) {
    desc.subpixel_mode = 0xEEEEEEEE;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenInvalidSadAdjustMode) {
    desc.sad_adjust_mode = 0xEEEEEEEE;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, givenInvalidSearchPathType) {
    desc.search_path_type = 0xEEEEEEEE;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationTest, createWithValidArgumentsExpectPass) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    ASSERT_NE(nullptr, accelerator);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto acc = static_cast<VmeAccelerator *>(accelerator);

    delete acc;
}

TEST_F(IntelMotionEstimationTest, createWithNullReturnExpectPass) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        nullptr);

    ASSERT_NE(nullptr, accelerator);

    auto acc = static_cast<VmeAccelerator *>(accelerator);

    delete acc;
}

TEST_F(IntelMotionEstimationTest, releaseAfterCreateExpectPass) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    ASSERT_NE(nullptr, accelerator);
    ASSERT_EQ(CL_SUCCESS, retVal);

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(IntelMotionEstimationTest, retainAfterCreateExpectPass) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    ASSERT_NE(nullptr, accelerator);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pAccelerator = static_cast<IntelAccelerator *>(accelerator);

    ASSERT_EQ(1, pAccelerator->getReference());

    result = clRetainAcceleratorINTEL(accelerator);
    ASSERT_EQ(CL_SUCCESS, result);
    ASSERT_EQ(2, pAccelerator->getReference());

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(1, pAccelerator->getReference());

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
}

struct IntelMotionEstimationGetInfoTest : public IntelMotionEstimationTest {
  public:
    IntelMotionEstimationGetInfoTest() : type_returned(static_cast<cl_accelerator_type_intel>(-1)),
                                         param_value_size_ret(static_cast<size_t>(-1)) {}

    void SetUp() override {
        IntelMotionEstimationTest::SetUp();

        descReturn.mb_block_type = static_cast<cl_uint>(-1);
        descReturn.subpixel_mode = static_cast<cl_uint>(-1);
        descReturn.sad_adjust_mode = static_cast<cl_uint>(-1);
        descReturn.search_path_type = static_cast<cl_uint>(-1);

        accelerator = clCreateAcceleratorINTEL(
            pContext,
            CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
            sizeof(cl_motion_estimation_desc_intel),
            &desc,
            &retVal);

        ASSERT_NE(nullptr, accelerator);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        result = clReleaseAcceleratorINTEL(accelerator);
        EXPECT_EQ(CL_SUCCESS, result);

        IntelMotionEstimationTest::TearDown();
    }

  protected:
    cl_motion_estimation_desc_intel descReturn;
    cl_accelerator_type_intel type_returned;
    size_t param_value_size_ret;
};

TEST_F(IntelMotionEstimationGetInfoTest, getInfoDescriptorExactExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel), // exact
        reinterpret_cast<void *>(&descReturn),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), param_value_size_ret);

    EXPECT_EQ(static_cast<cl_uint>(CL_ME_MB_TYPE_16x16_INTEL), descReturn.mb_block_type);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SUBPIXEL_MODE_QPEL_INTEL), descReturn.subpixel_mode);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SAD_ADJUST_MODE_HAAR_INTEL), descReturn.sad_adjust_mode);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL), descReturn.search_path_type);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoDescriptorShortExpectFail) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel) - 1, // short
        reinterpret_cast<void *>(&descReturn),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoDescriptorVeryShortExpectFail) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        0, // very short
        reinterpret_cast<void *>(&descReturn),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoDescriptorLongExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel) + 1, // long
        reinterpret_cast<void *>(&descReturn),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoDescriptorSizeQueryExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        0, // query required size w/nullptr return
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeExpectPass) {
    ASSERT_EQ(sizeof(cl_accelerator_type_intel), sizeof(cl_uint));

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint),
        reinterpret_cast<void *>(&type_returned),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);

    EXPECT_EQ(static_cast<cl_accelerator_type_intel>(CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL), type_returned);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeExactExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint), // exact
        reinterpret_cast<void *>(&type_returned),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);

    EXPECT_EQ(static_cast<cl_accelerator_type_intel>(CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL), type_returned);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeShortExpectFail) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint) - 1, // short
        reinterpret_cast<void *>(&type_returned),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeVeryShortExpectFail) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        0, // very short
        reinterpret_cast<void *>(&type_returned),
        &param_value_size_ret);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeLongExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint) + 1, // long
        reinterpret_cast<void *>(&type_returned),
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationGetInfoTest, getInfoTypeSizeQueryExpectPass) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), param_value_size_ret);
}

TEST_F(IntelMotionEstimationTest, createWith8x8IntegerNone2x2ExpectPass) {
    desc.mb_block_type = CL_ME_MB_TYPE_8x8_INTEL;
    desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_INTEGER_INTEL;
    desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_NONE_INTEL;
    desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(IntelMotionEstimationTest, createWith4x4HpelHaar16x12ExpectPass) {
    desc.mb_block_type = CL_ME_MB_TYPE_4x4_INTEL;
    desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_HPEL_INTEL;
    desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_HAAR_INTEL;
    desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_16_12_INTEL;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(IntelMotionEstimationTest, createWith16x16HpelHaar4x4ExpectPass) {
    desc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
    desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
    desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_HAAR_INTEL;
    desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_4_4_INTEL;

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    result = clReleaseAcceleratorINTEL(accelerator);
    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(IntelMotionEstimationNegativeTest, createWithInvalidBlockTypeExpectPass) {
    desc.mb_block_type = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, createWithInvalidSubpixelModeExpectPass) {
    desc.subpixel_mode = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, createWithInvalidAdjustModeExpectPass) {
    desc.sad_adjust_mode = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, createWithInvalidPathTypeExpectPass) {
    desc.search_path_type = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}
} // namespace ULT
