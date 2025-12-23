/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session.h"

namespace L0 {

DebugSession *createDebugSessionHelperXe(const zet_debug_config_t &config, Device *device, int debugFd, std::unique_ptr<NEO::EuDebugInterface> &&debugInterface, void *params) {
    return new DebugSessionLinuxXe(config, device, debugFd, std::move(debugInterface), params);
}

ze_result_t openConnectionUpstreamHelper(int pid, Device *device, NEO::EuDebugInterface &debugInterface, NEO::EuDebugConnect *open, int &debugFd) {
    return DebugSessionLinuxXe::openConnectionUpstream(pid, device, debugInterface, open, debugFd);
}

} // namespace L0
