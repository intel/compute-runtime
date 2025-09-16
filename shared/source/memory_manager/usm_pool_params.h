/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>

namespace NEO {
struct UsmPoolParams {
    size_t poolSize{0};
    size_t minServicedSize{0};
    size_t maxServicedSize{0};

    static UsmPoolParams getUsmPoolParams();
    static size_t getUsmPoolSize();
};
} // namespace NEO
