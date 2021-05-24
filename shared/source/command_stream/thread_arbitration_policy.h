/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>
namespace NEO {
namespace ThreadArbitrationPolicy {

enum : uint32_t {
    AgeBased = 0x0u,
    RoundRobin = 0x1u,
    RoundRobinAfterDependency = 0x2u,
    NotPresent = 0xffffu
};

} // namespace ThreadArbitrationPolicy
} // namespace NEO
