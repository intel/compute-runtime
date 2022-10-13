/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/image_set_arg_fixture.h"

GEN12LPTEST_F(ImageMediaBlockSetArgTest, givenKernelWithMediaBlockOperationWhenSupportedFormatPassedThenSetKernelArgReturnSuccess) {
    const_cast<ClSurfaceFormatInfo &>(srcImage->getSurfaceFormatInfo()).surfaceFormat.GenxSurfaceFormat = NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

GEN12LPTEST_F(ImageMediaBlockSetArgTest, givenKernelWithMediaBlockOperationWhenUnSupportedFormatPassedThenSetKernelArgReturnErrCode) {
    const_cast<ClSurfaceFormatInfo &>(srcImage->getSurfaceFormatInfo()).surfaceFormat.GenxSurfaceFormat = NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT;
    cl_mem memObj = srcImage;

    retVal = clSetKernelArg(
        pMultiDeviceKernel,
        0,
        sizeof(memObj),
        &memObj);
    ASSERT_EQ(CL_INVALID_ARG_VALUE, retVal);
}
