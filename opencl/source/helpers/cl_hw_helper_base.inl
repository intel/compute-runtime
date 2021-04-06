/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/program/kernel_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return hasStatelessAccessToBuffer(kernelInfo);
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const {
    bool hasStatelessAccessToBuffer = false;
    for (uint32_t i = 0; i < kernelInfo.kernelArgInfo.size(); ++i) {
        if (kernelInfo.kernelArgInfo[i].isBuffer) {
            hasStatelessAccessToBuffer |= !kernelInfo.kernelArgInfo[i].pureStatefulBufferAccess;
        }
    }
    return hasStatelessAccessToBuffer;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::allowRenderCompressionForContext(const ClDevice &clDevice, const Context &context) const {
    return true;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::getQueueFamilyName(std::string &name, EngineGroupType type) const {
    return false;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::preferBlitterForLocalToLocalTransfers() const {
    return false;
}
template <typename GfxFamily>
bool ClHwHelperHw<GfxFamily>::isSupportedKernelThreadArbitrationPolicy() const { return true; }

template <typename GfxFamily>
std::vector<uint32_t> ClHwHelperHw<GfxFamily>::getSupportedThreadArbitrationPolicies() const {
    return std::vector<uint32_t>{CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL};
}

} // namespace NEO
