/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class SubmissionStatus : uint32_t {
    success = 0,
    failed,
    outOfMemory,
    outOfHostMemory,
    unsupported,
    deviceUninitialized,
};

} // namespace NEO
