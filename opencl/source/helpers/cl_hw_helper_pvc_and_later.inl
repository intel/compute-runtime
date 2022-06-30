/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/extensions/public/cl_ext_private.h"

template <>
inline bool ClHwHelperHw<Family>::preferBlitterForLocalToLocalTransfers() const {
    return false;
}

template <>
std::vector<uint32_t> ClHwHelperHw<Family>::getSupportedThreadArbitrationPolicies() const {
    return std::vector<uint32_t>{CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL};
}

template <>
bool ClHwHelperHw<Family>::allowImageCompression(cl_image_format format) const {
    return true;
}
