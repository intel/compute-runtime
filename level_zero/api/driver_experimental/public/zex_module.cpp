/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"

namespace L0 {

ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress) {
    return L0::Kernel::fromHandle(hKernel)->getBaseAddress(baseAddress);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress) {
    return L0::zexKernelGetBaseAddress(hKernel, baseAddress);
}
}
