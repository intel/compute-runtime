/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class EvictionStatus : uint32_t {
    SUCCESS = 0,
    FAILED,
    NOT_APPLIED,
    UNKNOWN
};

} // namespace NEO
