/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct DebugData {
    uint32_t vIsaSize = 0;
    uint32_t genIsaSize = 0;
    const char *vIsa = nullptr;
    const char *genIsa = nullptr;
};
} // namespace NEO
