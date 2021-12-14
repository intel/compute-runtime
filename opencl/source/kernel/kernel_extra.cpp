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

int32_t Kernel::setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) {
    return CL_INVALID_VALUE;
}

} // namespace NEO
