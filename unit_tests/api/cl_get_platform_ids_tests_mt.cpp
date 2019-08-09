/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/platform/platform.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetPlatformIDsMtTests;

namespace ULT {

TEST_F(clGetPlatformIDsMtTests, GivenSeparateThreadWhenGettingPlatformIdThenPlatformIdIsCorrect) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;
    cl_platform_id threadPlatform = nullptr;

    std::thread t1([&] { clGetPlatformIDs(1, &threadPlatform, nullptr); });

    retVal = clGetPlatformIDs(1, &platform, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    t1.join();

    EXPECT_EQ(threadPlatform, platform);
}
} // namespace ULT
