/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
namespace HostClock {
using TimestampNs = int64_t;

TimestampNs getCurrentTimeInNs();
int64_t getElapsedTimeInUs(TimestampNs startTimeInNs, TimestampNs endTimeInNs);
} // namespace HostClock
} // namespace NEO
