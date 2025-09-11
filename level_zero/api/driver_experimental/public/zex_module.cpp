/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {

ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress) {
    return L0::Kernel::fromHandle(toInternalType(hKernel))->getBaseAddress(baseAddress);
}

ze_result_t ZE_APICALL
zexKernelGetArgumentSize(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pArgSize) {
    return L0::Kernel::fromHandle(toInternalType(hKernel))->getArgumentSize(argIndex, pArgSize);
}

ze_result_t ZE_APICALL
zexKernelGetArgumentType(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pSize,
    char *pString) {
    return L0::Kernel::fromHandle(toInternalType(hKernel))->getArgumentType(argIndex, pSize, pString);
}

ze_result_t ZE_APICALL
zeIntelKernelGetBinaryExp(
    ze_kernel_handle_t hKernel, size_t *pSize, char *pKernelBinary) {
    return L0::Kernel::fromHandle(toInternalType(hKernel))->getKernelProgramBinary(pSize, pKernelBinary);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeIntelKernelGetBinaryExp(
    ze_kernel_handle_t hKernel, size_t *pSize, char *pKernelBinary) {
    return L0::zeIntelKernelGetBinaryExp(hKernel, pSize, pKernelBinary);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexKernelGetBaseAddress(
    ze_kernel_handle_t hKernel,
    uint64_t *baseAddress) {
    return L0::zexKernelGetBaseAddress(hKernel, baseAddress);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexKernelGetArgumentSize(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pArgSize) {
    return L0::zexKernelGetArgumentSize(hKernel, argIndex, pArgSize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexKernelGetArgumentType(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    uint32_t *pSize,
    char *pString) {
    return L0::zexKernelGetArgumentType(hKernel, argIndex, pSize, pString);
}
} // extern "C"
