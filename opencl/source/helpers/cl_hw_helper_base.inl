/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/program/kernel_info.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"

namespace NEO {

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::requiresNonAuxMode(const ArgDescPointer &argAsPtr, const HardwareInfo &hwInfo) const {
    return !argAsPtr.isPureStateful();
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::requiresAuxResolves(const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) const {
    return hasStatelessAccessToBuffer(kernelInfo);
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const {
    for (const auto &arg : kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) {
        if (arg.is<ArgDescriptor::ArgTPointer>() && !arg.as<ArgDescPointer>().isPureStateful()) {
            return true;
        }
    }
    return false;
}

template <typename GfxFamily>
inline bool ClHwHelperHw<GfxFamily>::allowCompressionForContext(const ClDevice &clDevice, const Context &context) const {
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

template <typename GfxFamily>
bool ClHwHelperHw<GfxFamily>::allowImageCompression(cl_image_format format) const {
    return true;
}

} // namespace NEO
