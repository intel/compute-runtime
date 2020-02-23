/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>
namespace NEO {

namespace ThreadArbitrationPolicy {
enum {
    AgeBased = 0x0u,
    RoundRobin = 0x1u,
    RoundRobinAfterDependency = 0x2u,
    NotPresent = 0xffffffffu
};
} // namespace ThreadArbitrationPolicy
} // namespace NEO
