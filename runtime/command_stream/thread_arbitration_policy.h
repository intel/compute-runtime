/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>
namespace OCLRT {
struct ThreadArbitrationPolicy {
    enum {
        AgeBased = 0x0u,
        RoundRobin = 0x1u,
        RoundRobinAfterDependency = 0x2u,
        NotPresent = 0xffffffffu
    };
};
} // namespace OCLRT
