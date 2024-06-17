/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/queue_throttle.h"

namespace NEO {
QueueThrottle getThrottleFromPowerSavingUint(uint8_t value) {
    if (value == 0) {
        return QueueThrottle::MEDIUM;
    } else {
        return QueueThrottle::LOW;
    }
}
} // namespace NEO