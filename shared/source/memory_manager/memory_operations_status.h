/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class MemoryOperationsStatus : uint32_t {
    SUCCESS = 0,
    FAILED,
    MEMORY_NOT_FOUND,
    OUT_OF_MEMORY,
    UNSUPPORTED,
    DEVICE_UNINITIALIZED,
    GPU_HANG_DETECTED_DURING_OPERATION,
};

} // namespace NEO
