/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum class SubmissionStatus : uint32_t {
    SUCCESS = 0,
    FAILED,
    OUT_OF_MEMORY,
    UNSUPPORTED,
    DEVICE_UNINITIALIZED,
};

} // namespace NEO
