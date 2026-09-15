/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress);

ze_result_t ZE_APICALL
zexKernelGetArgumentSize(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pArgSize);

ze_result_t ZE_APICALL
zexKernelGetArgumentType(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pSize,
    char *pString);

ze_result_t ZE_APICALL
zeIntelKernelGetBinaryExp(
    ze_kernel_handle_t hKernel, size_t *pSize, char *pKernelBinary);

ze_result_t ZE_APICALL
zeKernelGetModuleHandleExt(
    ze_kernel_handle_t hKernel, ze_module_handle_t *phModule);

ze_result_t ZE_APICALL
zeModuleGetDeviceHandleExt(
    ze_module_handle_t hModule, ze_device_handle_t *phDevice);

} // namespace L0
