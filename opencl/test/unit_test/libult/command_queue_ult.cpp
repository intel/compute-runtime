/*
 * Copyright (C) 2020-2022 Intel Corporation
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

void CommandQueue::finishBeforeRelease() {
    *this->getHwTagAddress() = this->taskCount;
    this->finish();
}

} // namespace NEO