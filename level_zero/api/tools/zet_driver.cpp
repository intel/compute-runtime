/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tools_init.h"
#include <level_zero/zet_api.h>

#include <iostream>

extern "C" {

__zedllexport ze_result_t __zecall
zetInit(
    ze_init_flag_t flags) {
    try {
        {
        }
        return L0::ToolsInit::get()->initTools(flags);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
