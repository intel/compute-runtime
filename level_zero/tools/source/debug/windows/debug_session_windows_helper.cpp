/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/windows/debug_session.h"
#include <level_zero/ze_api.h>
namespace L0 {

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd) {
    return new DebugSessionWindows(config, device);
}

} // namespace L0
