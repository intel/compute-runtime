/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

namespace L0 {

DebugSession *createDebugSessionHelperXe(const zet_debug_config_t &config, Device *device, int debugFd, std::unique_ptr<NEO::EuDebugInterface> debugInterface, void *params) {
    return new DebugSessionLinuxXe(config, device, debugFd, std::move(debugInterface), params);
}

} // namespace L0
