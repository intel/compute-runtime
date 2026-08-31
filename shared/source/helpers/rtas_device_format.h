/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

// Ray tracing acceleration structure format consumed by the RTAS builder library.
enum class RTASDeviceFormat : uint32_t {
    invalid = 0,
    version1 = 1,
    version2 = 2,
    version3 = 3,
    version4 = 4,
    version5 = 5,
};

} // namespace NEO
