/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module.h"
#include <level_zero/zet_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zetModuleGetDebugInfo(
    zet_module_handle_t hModule,
    zet_module_debug_info_format_t format,
    size_t *pSize,
    uint8_t *pDebugInfo) {
    return L0::Module::fromHandle(hModule)->getDebugInfo(pSize, pDebugInfo);
}

} // extern C