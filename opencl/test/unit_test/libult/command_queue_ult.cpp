/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isAssignEngineRoundRobinEnabled() {
    auto assignEngineRoundRobin = false;

    if (DebugManager.flags.EnableCmdQRoundRobindEngineAssign.get() != -1) {
        assignEngineRoundRobin = DebugManager.flags.EnableCmdQRoundRobindEngineAssign.get();
    }

    return assignEngineRoundRobin;
}

bool CommandQueue::isTimestampWaitEnabled() {
    return ultHwConfig.useWaitForTimestamps;
}

} // namespace NEO