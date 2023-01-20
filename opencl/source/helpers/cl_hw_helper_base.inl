/*
 * Copyright (C) 2020-2023 Intel Corporation
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
inline bool ClGfxCoreHelperHw<GfxFamily>::requiresNonAuxMode(const ArgDescPointer &argAsPtr) const {
    return !argAsPtr.isPureStateful();
}

template <typename GfxFamily>
inline bool ClGfxCoreHelperHw<GfxFamily>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return hasStatelessAccessToBuffer(kernelInfo);
}

template <typename GfxFamily>
inline bool ClGfxCoreHelperHw<GfxFamily>::hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const {
    for (const auto &arg : kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) {
        if (arg.is<ArgDescriptor::ArgTPointer>() && !arg.as<ArgDescPointer>().isPureStateful()) {
            return true;
        }
    }
    return false;
}

template <typename GfxFamily>
inline bool ClGfxCoreHelperHw<GfxFamily>::allowCompressionForContext(const ClDevice &clDevice, const Context &context) const {
    return true;
}

template <typename GfxFamily>
inline bool ClGfxCoreHelperHw<GfxFamily>::getQueueFamilyName(std::string &name, EngineGroupType type) const {
    return false;
}

template <typename GfxFamily>
inline bool ClGfxCoreHelperHw<GfxFamily>::preferBlitterForLocalToLocalTransfers() const {
    return false;
}
template <typename GfxFamily>
bool ClGfxCoreHelperHw<GfxFamily>::isSupportedKernelThreadArbitrationPolicy() const { return true; }

template <typename GfxFamily>
std::vector<uint32_t> ClGfxCoreHelperHw<GfxFamily>::getSupportedThreadArbitrationPolicies() const {
    return std::vector<uint32_t>{CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL};
}

template <typename GfxFamily>
bool ClGfxCoreHelperHw<GfxFamily>::allowImageCompression(cl_image_format format) const {
    return true;
}

} // namespace NEO
