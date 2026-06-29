/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class UserFenceWaitOperation : uint32_t {
    notEqual,
    greaterOrEqual,
};

enum class UserFenceValueWidth : uint32_t {
    u32,
    u64,
};

} // namespace NEO
