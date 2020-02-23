/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/thread_arbitration_policy.h"

#include "gtest/gtest.h"
#include "public/cl_ext_private.h"

namespace NEO {
TEST(ThreadArbitrationPolicy, givenClKrenelExecThreadArbitrationPolicyWhenGetNewKernelArbitrationPolicyIsCalledThenExpectedThreadArbitrationPolicyIsReturned) {
    uint32_t retVal = ThreadArbitrationPolicy::getNewKernelArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL);
    EXPECT_EQ(retVal, static_cast<uint32_t>(ThreadArbitrationPolicy::RoundRobin));
    retVal = ThreadArbitrationPolicy::getNewKernelArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL);
    EXPECT_EQ(retVal, static_cast<uint32_t>(ThreadArbitrationPolicy::AgeBased));
    retVal = ThreadArbitrationPolicy::getNewKernelArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL);
    EXPECT_EQ(retVal, static_cast<uint32_t>(ThreadArbitrationPolicy::RoundRobinAfterDependency));
    uint32_t randomValue = -1;
    retVal = ThreadArbitrationPolicy::getNewKernelArbitrationPolicy(randomValue);
    EXPECT_EQ(retVal, static_cast<uint32_t>(ThreadArbitrationPolicy::NotPresent));
}
} // namespace NEO