/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"
#include <level_zero/ze_api.h>
namespace L0 {

DebugSession *createDebugSessionHelperXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params) {
    return new DebugSessionLinuxXe(config, device, debugFd, params);
}

} // namespace L0
