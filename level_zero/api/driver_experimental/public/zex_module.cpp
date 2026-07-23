/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_module.h"

#include "level_zero/api/internal/l0_module.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
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

ze_result_t ZE_APICALL
zeKernelGetModuleHandleExt(
    ze_kernel_handle_t hKernel,
    ze_module_handle_t *phModule) {
    auto kernel = L0::Kernel::fromHandle(toInternalType(hKernel));
    if (nullptr == kernel) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (nullptr == phModule) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    *phModule = kernel->getModule()->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL
zeModuleGetDeviceHandleExt(
    ze_module_handle_t hModule,
    ze_device_handle_t *phDevice) {
    auto module = L0::Module::fromHandle(toInternalType(hModule));
    if (nullptr == module) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (nullptr == phDevice) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    *phDevice = module->getDevice()->toHandle();
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
