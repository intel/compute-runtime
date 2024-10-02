/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct SizeToPreferredSlmValue {
    uint32_t upperLimit;
    uint32_t valueToProgram;
};
} // namespace NEO
