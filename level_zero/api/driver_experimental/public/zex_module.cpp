/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"

ZE_DLLEXPORT ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress) {
    return L0::Kernel::fromHandle(hKernel)->getBaseAddress(baseAddress);
}
