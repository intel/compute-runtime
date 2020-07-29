/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module.h"
#include <level_zero/zet_api.h>

#include "third_party/level_zero/zet_api_ext.h"

extern "C" {

__zedllexport ze_result_t __zecall
zetModuleGetDebugInfo(
    zet_module_handle_t hModule,
    zet_module_debug_info_format_t format,
    size_t *pSize,
    uint8_t *pDebugInfo) {
    return L0::Module::fromHandle(hModule)->getDebugInfo(pSize, pDebugInfo);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetKernelGetProfileInfoExt(
    zet_kernel_handle_t hKernel,
    zet_profile_properties_t *pProfileProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // extern C
