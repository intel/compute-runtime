/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/extensions/public/cl_ext_private.h"

template <>
std::vector<uint32_t> ClGfxCoreHelperHw<Family>::getSupportedThreadArbitrationPolicies() const {
    return std::vector<uint32_t>{CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL};
}

template <>
inline bool ClGfxCoreHelperHw<Family>::getQueueFamilyName(std::string &name, EngineGroupType type) const {
    switch (type) {
    case EngineGroupType::renderCompute:
        name = "cccs";
        return true;
    case EngineGroupType::linkedCopy:
        name = "linked bcs";
        return true;
    default:
        return false;
    }
}

template <>
bool ClGfxCoreHelperHw<Family>::allowImageCompression(cl_image_format format) const {
    return true;
}
