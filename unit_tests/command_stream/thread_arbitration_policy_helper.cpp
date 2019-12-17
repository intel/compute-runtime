/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/command_stream/thread_arbitration_policy.h"

#include <stdint.h>
namespace NEO {
uint32_t getNewKernelArbitrationPolicy(uint32_t policy) {
    if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL) {
        return ThreadArbitrationPolicy::RoundRobin;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL) {
        return ThreadArbitrationPolicy::AgeBased;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL) {
        return ThreadArbitrationPolicy::RoundRobinAfterDependency;
    } else {
        return ThreadArbitrationPolicy::NotPresent;
    }
}
} // namespace NEO
