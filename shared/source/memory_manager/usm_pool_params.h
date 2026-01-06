/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>

namespace NEO {
class GfxCoreHelper;
struct UsmPoolParams {
    size_t poolSize{0};
    size_t minServicedSize{0};
    size_t maxServicedSize{0};

    static UsmPoolParams getUsmPoolParams(const GfxCoreHelper &gfxCoreHelper);
    static size_t getUsmPoolSize(const GfxCoreHelper &gfxCoreHelper);
};
} // namespace NEO
