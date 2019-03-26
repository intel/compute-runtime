/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetSupportedImageFormatsTests;

TEST_F(clGetSupportedImageFormatsTests, returnsNumReadWriteFormats) {
    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        pContext,
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}

TEST_F(clGetSupportedImageFormatsTests, givenInvalidContextWhenFunctionIsCalledThenErrorIsReturned) {
    auto device = pContext->getDevice(0u);
    auto dummyContext = reinterpret_cast<cl_context>(device);

    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        dummyContext,
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
