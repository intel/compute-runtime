/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/host_clock.h"

#include <chrono>

namespace NEO {
namespace HostClock {
TimestampNs getCurrentTimeInNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int64_t getElapsedTimeInNs(TimestampNs startTimeInNs, TimestampNs endTimeInNs) {
    return endTimeInNs - startTimeInNs;
}

int64_t getElapsedTimeInUs(TimestampNs startTimeInNs, TimestampNs endTimeInNs) {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds{endTimeInNs - startTimeInNs}).count();
}

int64_t getElapsedTimeInMs(TimestampNs startTimeInNs, TimestampNs endTimeInNs) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds{endTimeInNs - startTimeInNs}).count();
}
} // namespace HostClock
} // namespace NEO
