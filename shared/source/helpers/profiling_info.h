/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct ProfilingInfo {
    uint64_t cpuTimeInNs;
    uint64_t gpuTimeInNs;
    uint64_t gpuTimeStamp;
};

} // namespace NEO
