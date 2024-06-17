/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
enum QueueThrottle : uint32_t {
    LOW,
    MEDIUM,
    HIGH
};

QueueThrottle getThrottleFromPowerSavingUint(uint8_t value);
} // namespace NEO
