/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
using FlushStamp = uint64_t;
enum class SubmissionStatus : uint32_t;
struct CompletionStamp {
    static uint32_t getTaskCountFromSubmissionStatusError(SubmissionStatus submissionStatus);

    uint32_t taskCount;
    uint32_t taskLevel;
    FlushStamp flushStamp;

    static constexpr uint32_t notReady = 0xFFFFFFF0;
    static constexpr uint32_t gpuHang = 0xFFFFFFFA;
    static constexpr uint32_t outOfDeviceMemory = 0xFFFFFFFB;
    static constexpr uint32_t outOfHostMemory = 0xFFFFFFFC;
};

} // namespace NEO
