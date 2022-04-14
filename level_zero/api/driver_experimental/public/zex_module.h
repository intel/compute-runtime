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

#if defined(__cplusplus)
extern "C" {
#endif

#include "level_zero/api/driver_experimental/public/zex_api.h"

ZE_DLLEXPORT ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_MODULE_H
