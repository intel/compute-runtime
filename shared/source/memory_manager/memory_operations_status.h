/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class MemoryOperationsStatus : uint32_t {
    success = 0,
    failed,
    memoryNotFound,
    outOfMemory,
    unsupported,
    deviceUninitialized,
    gpuHangDetectedDuringOperation,
};

} // namespace NEO
