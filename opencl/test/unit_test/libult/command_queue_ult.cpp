/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

bool CommandQueue::isTimestampWaitEnabled() {
    return ultHwConfig.useWaitForTimestamps;
}

} // namespace NEO