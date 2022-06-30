/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/thread_arbitration_policy.h"

#include "opencl/extensions/public/cl_ext_private.h"

#include <stdint.h>
namespace NEO {
int32_t getNewKernelArbitrationPolicy(uint32_t policy) {
    if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL) {
        return ThreadArbitrationPolicy::RoundRobin;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL) {
        return ThreadArbitrationPolicy::AgeBased;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL || policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL) {
        return ThreadArbitrationPolicy::RoundRobinAfterDependency;
    } else {
        return ThreadArbitrationPolicy::NotPresent;
    }
}
} // namespace NEO
