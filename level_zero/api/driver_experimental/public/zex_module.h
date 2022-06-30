/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_MODULE_H
#define _ZEX_MODULE_H
#if defined(__cplusplus)
#pragma once
#endif

#include "level_zero/api/driver_experimental/public/zex_api.h"
namespace L0 {
ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress);

}

#endif // _ZEX_MODULE_H
