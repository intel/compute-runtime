/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

namespace L0 {
struct DebugSessionLinux : DebugSessionImp {

    DebugSessionLinux(const zet_debug_config_t &config, Device *device) : DebugSessionImp(config, device){};
    static ze_result_t translateDebuggerOpenErrno(int error);
};
} // namespace L0