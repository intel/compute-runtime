/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
#include <level_zero/zet_api.h>

namespace L0 {
ze_result_t zetModuleGetDebugInfo(
    zet_module_handle_t hModule,
    zet_module_debug_info_format_t format,
    size_t *pSize,
    uint8_t *pDebugInfo) {
    return L0::Module::fromHandle(hModule)->getDebugInfo(pSize, pDebugInfo);
}

ze_result_t zetKernelGetProfileInfo(
    zet_kernel_handle_t hKernel,
    zet_profile_properties_t *pProfileProperties) {
    return L0::Kernel::fromHandle(hKernel)->getProfileInfo(pProfileProperties);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zetModuleGetDebugInfo(
    zet_module_handle_t hModule,
    zet_module_debug_info_format_t format,
    size_t *pSize,
    uint8_t *pDebugInfo) {
    return L0::zetModuleGetDebugInfo(
        hModule,
        format,
        pSize,
        pDebugInfo);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetKernelGetProfileInfo(
    zet_kernel_handle_t hKernel,
    zet_profile_properties_t *pProfileProperties) {
    return L0::zetKernelGetProfileInfo(hKernel, pProfileProperties);
}
}
