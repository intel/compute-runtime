/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using clGetPlatformIDsMtTests = Test<PlatformFixture>;

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
