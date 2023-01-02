/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isAssignEngineRoundRobinEnabled() {
    return true;
}

bool CommandQueue::isTimestampWaitEnabled() {
    return true;
}

} // namespace NEO
