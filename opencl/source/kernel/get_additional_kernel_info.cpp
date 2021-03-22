/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"

namespace NEO {
void Kernel::getAdditionalInfo(cl_kernel_info paramName, const void *&paramValue, size_t &paramValueSizeRet) const {
}

void Kernel::getAdditionalWorkGroupInfo(cl_kernel_work_group_info paramName, const void *&paramValue, size_t &paramValueSizeRet) const {
}
} // namespace NEO
