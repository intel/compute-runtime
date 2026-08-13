/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class FreePolicyType : uint32_t {
    none = 0,
    blocking = 1,
    defer = 2
};

} // namespace NEO
