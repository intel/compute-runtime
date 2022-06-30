/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum ThreadArbitrationPolicy : int32_t {
    AgeBased = 0,
    RoundRobin = 1,
    RoundRobinAfterDependency = 2,
    NotPresent = -1
};

} // namespace NEO
