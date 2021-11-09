/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {
bool CommandQueue::isAssignEngineRoundRobinEnabled() {
    return false;
}
} // namespace NEO