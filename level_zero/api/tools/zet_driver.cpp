/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tools_init.h"
#include <level_zero/zet_api.h>

#include "third_party/level_zero/ze_api_ext.h"

#include <iostream>

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zetInit(
    ze_init_flag_t flags) {
    return L0::ToolsInit::get()->initTools(flags);
}

} // extern C
