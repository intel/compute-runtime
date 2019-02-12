/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace OCLRT {
enum PreemptionMode : uint32_t {
    // Keep in sync with ForcePreemptionMode debug variable
    Initial = 0,
    Disabled = 1,
    MidBatch,
    ThreadGroup,
    MidThread,
};
} // namespace OCLRT
