/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/kernel/kernel.h"

namespace NEO {
bool Kernel::requiresCacheFlushCommand(const CommandQueue &commandQueue) const {
    return false;
}
void Kernel::reconfigureKernel() {
}
int Kernel::setKernelThreadArbitrationPolicy(uint32_t policy) {
    if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL) {
        this->threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL) {
        this->threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL) {
        this->threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    } else {
        this->threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

bool Kernel::requiresPerDssBackedBuffer() const {
    return DebugManager.flags.ForcePerDssBackedBufferProgramming.get();
}

} // namespace NEO