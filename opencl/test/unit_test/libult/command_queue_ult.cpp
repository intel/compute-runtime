/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isAssignEngineRoundRobinEnabled() {
    return false;
}

bool CommandQueue::isTimestampWaitEnabled() {
    return ultHwConfig.useWaitForTimestamps;
}

bool checkIsGpuCopyRequiredForDcFlushMitigation(AllocationType type) {
    return ultHwConfig.useGpuCopyForDcFlushMitigation;
}

} // namespace NEO