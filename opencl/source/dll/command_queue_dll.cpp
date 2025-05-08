/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isTimestampWaitEnabled() {
    return true;
}

} // namespace NEO
