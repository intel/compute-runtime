/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {

enum DebugSessionLinuxType {
    DEBUG_SESSION_LINUX_TYPE_I915 = 0,
    DEBUG_SESSION_LINUX_TYPE_XE = 1,
    DEBUG_SESSION_LINUX_TYPE_MAX = 2
};

using DebugSessionLinuxAllocatorFn = DebugSession *(*)(const zet_debug_config_t &, Device *, ze_result_t &, bool);
extern DebugSessionLinuxAllocatorFn debugSessionLinuxFactory[];

template <uint32_t driverType, typename DebugSessionType>
struct DebugSessionLinuxPopulateFactory {
    DebugSessionLinuxPopulateFactory() {
        debugSessionLinuxFactory[driverType] = DebugSessionType::createLinuxSession;
    }
};

} // namespace L0