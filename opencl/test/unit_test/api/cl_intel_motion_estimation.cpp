/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct IntelMotionEstimationTest : public ApiTests {
  public:
    IntelMotionEstimationTest() {}

    void SetUp() override {
        ApiTests::SetUp();

        desc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
        desc.subpixel_mode = CL_ME_SUBPIXEL_MODE_QPEL_INTEL;
        desc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_HAAR_INTEL;
        desc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;
    }

    void TearDown() override {
        ApiTests::TearDown();
    }

  protected:
    cl_accelerator_intel accelerator = nullptr;
    cl_motion_estimation_desc_intel desc;
    cl_int retVal = 0xEEEEEEEEu;
    cl_int result = -1;
};

typedef IntelMotionEstimationTest IntelMotionEstimationNegativeTest;

TEST_F(IntelMotionEstimationNegativeTest, GivenNullDescriptorWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenDescriptorSizeLongerThanActualWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel) + 1,
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenDescriptorSizeShorterThanActualWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel) - 1,
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
    ASSERT_EQ(static_cast<cl_accelerator_intel>(nullptr), accelerator);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidMacroBlockTypeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
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

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidSubPixelModeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
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

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidSadAdjustModeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
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

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidSearchPathTypeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
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

TEST_F(IntelMotionEstimationTest, GivenValidArgumentsWhenCreatingAcceleratorThenAcceleratorIsCreated) {
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

TEST_F(IntelMotionEstimationTest, GivenNullReturnWhenCreatingAcceleratorThenAcceleratorIsCreated) {
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

TEST_F(IntelMotionEstimationTest, GivenValidAcceleratorWhenReleasingAcceleratorThenSuccessIsReturned) {
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

TEST_F(IntelMotionEstimationTest, GivenValidAcceleratorWhenRetainingAndReleasingAcceleratorThenReferenceCountIsAdjustedCorrectly) {
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
    IntelMotionEstimationGetInfoTest() : typeReturned(static_cast<cl_accelerator_type_intel>(-1)),
                                         paramValueSizeRet(static_cast<size_t>(-1)) {}

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
    cl_accelerator_type_intel typeReturned;
    size_t paramValueSizeRet;
};

TEST_F(IntelMotionEstimationGetInfoTest, GivenValidParamsWhenGettingAcceleratorInfoThenDescriptorContainsCorrectInformation) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel), // exact
        &descReturn,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), paramValueSizeRet);

    EXPECT_EQ(static_cast<cl_uint>(CL_ME_MB_TYPE_16x16_INTEL), descReturn.mb_block_type);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SUBPIXEL_MODE_QPEL_INTEL), descReturn.subpixel_mode);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SAD_ADJUST_MODE_HAAR_INTEL), descReturn.sad_adjust_mode);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL), descReturn.search_path_type);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenTooShortDescriptorLengthWhenGettingAcceleratorInfoThenClInvalidValueErrorIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel) - 1, // short
        &descReturn,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenDescriptorLengthZeroWhenGettingAcceleratorInfoThenClInvalidValueErrorIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        0,
        &descReturn,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenInvalidParametersWhenGettingAcceleratorInfoThenValueSizeRetIsNotUpdated) {
    paramValueSizeRet = 0x1234;

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        0,
        &descReturn,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenLongerDescriptorLengthWhenGettingAcceleratorInfoThenCorrectDescriptorLengthIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        sizeof(cl_motion_estimation_desc_intel) + 1, // long
        &descReturn,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), paramValueSizeRet);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenDescriptorLengthZeroAndDescriptorNullWhenGettingAcceleratorInfoThenCorrectDescriptorLengthIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_DESCRIPTOR_INTEL,
        0, // query required size w/nullptr return
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_motion_estimation_desc_intel), paramValueSizeRet);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeWhenGettingAcceleratorInfoThenAcceleratorTypeMotionEstimationIntelIsReturned) {
    ASSERT_EQ(sizeof(cl_accelerator_type_intel), sizeof(cl_uint));

    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint),
        &typeReturned,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), paramValueSizeRet);

    EXPECT_EQ(static_cast<cl_accelerator_type_intel>(CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL), typeReturned);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeIntelWhenGettingAcceleratorInfoThenClAcceleratorTypeMotionEstimationIntelIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint), // exact
        &typeReturned,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), paramValueSizeRet);

    EXPECT_EQ(static_cast<cl_accelerator_type_intel>(CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL), typeReturned);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeIntelAndTooShortTypeLengthWhenGettingAcceleratorInfoThenClInvalidValueIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint) - 1, // short
        &typeReturned,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeIntelAndTypeLengthZeroWhenGettingAcceleratorInfoThenClInvalidValueIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        0, // very short
        &typeReturned,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeIntelAndTooLongTypeLengthWhenGettingAcceleratorInfoThenCorrectLengthIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        sizeof(cl_uint) + 1, // long
        &typeReturned,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), paramValueSizeRet);
}

TEST_F(IntelMotionEstimationGetInfoTest, GivenAcceleratorTypeIntelAndNullTypeWhenGettingAcceleratorInfoThenCorrectLengthIsReturned) {
    result = clGetAcceleratorInfoINTEL(
        accelerator,
        CL_ACCELERATOR_TYPE_INTEL,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(sizeof(cl_accelerator_type_intel), paramValueSizeRet);
}

TEST_F(IntelMotionEstimationTest, GivenDescriptor8x8IntegerNone2x2WhenCreatingAcceleratorThenSuccessIsReturned) {
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

TEST_F(IntelMotionEstimationTest, GivenDescriptor4x4HpelHaar16x12WhenCreatingAcceleratorThenSuccessIsReturned) {
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

TEST_F(IntelMotionEstimationTest, GivenDescriptor16x16HpelHaar4x4WhenCreatingAcceleratorThenSuccessIsReturned) {
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

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidBlockTypeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    desc.mb_block_type = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidSubpixelModeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    desc.subpixel_mode = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidAdjustModeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
    desc.sad_adjust_mode = static_cast<cl_uint>(-1);

    accelerator = clCreateAcceleratorINTEL(
        pContext,
        CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL,
        sizeof(cl_motion_estimation_desc_intel),
        &desc,
        &retVal);

    EXPECT_EQ(CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL, retVal);
}

TEST_F(IntelMotionEstimationNegativeTest, GivenInvalidPathTypeWhenCreatingAcceleratorThenClInvalidAcceleratorDescriptorIntelErrorIsReturned) {
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
