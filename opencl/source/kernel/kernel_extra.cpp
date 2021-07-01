/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

int Kernel::setKernelThreadArbitrationPolicy(uint32_t policy) {
    auto hwInfo = clDevice.getHardwareInfo();
    auto &hwHelper = NEO::ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        this->threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        return CL_INVALID_DEVICE;
    } else if (policy == CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL) {
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

int32_t Kernel::setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) {
    return CL_INVALID_VALUE;
}

} // namespace NEO
