/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isAssignEngineRoundRobinEnabled() {
    return true;
}

bool CommandQueue::isTimestampWaitEnabled() {
    return true;
}

bool checkIsGpuCopyRequiredForDcFlushMitigation(AllocationType type) {
    return type != AllocationType::bufferHostMemory;
}

} // namespace NEO
