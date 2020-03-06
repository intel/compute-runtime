/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module.h"
#include <level_zero/zet_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zetModuleGetDebugInfo(
    zet_module_handle_t hModule,
    zet_module_debug_info_format_t format,
    size_t *pSize,
    uint8_t *pDebugInfo) {
    {
        if (nullptr == hModule)
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        if (nullptr == pSize)
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        if (format != ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF)
            return ZE_RESULT_ERROR_UNKNOWN;
    }
    return L0::Module::fromHandle(hModule)->getDebugInfo(pSize, pDebugInfo);
}
}