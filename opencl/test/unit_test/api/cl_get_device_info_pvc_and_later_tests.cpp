/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using clGetDeviceInfoPvcAndLater = ApiTests;
using matcherPvcAndLater = IsAtLeastXeHpcCore;
namespace ULT {
HWTEST2_F(clGetDeviceInfoPvcAndLater, givenClDeviceSupportedThreadArbitrationPolicyIntelWhenPvcAndLatereAndCallClGetDeviceInfoThenProperArrayIsReturned, matcherPvcAndLater) {
    cl_device_info paramName = 0;
    cl_uint paramValue[4];
    size_t paramSize = sizeof(paramValue);
    size_t paramRetSize = 0;

    paramName = CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL;
    cl_uint expectedRetValue[] = {CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL};

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(expectedRetValue), paramRetSize);
    EXPECT_TRUE(memcmp(expectedRetValue, paramValue, sizeof(expectedRetValue)) == 0);
}

} // namespace ULT
