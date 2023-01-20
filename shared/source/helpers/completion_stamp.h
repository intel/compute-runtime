/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"

#include <cstdint>
#include <limits>

namespace NEO {
using FlushStamp = uint64_t;
enum class SubmissionStatus : uint32_t;
struct CompletionStamp {
    static TaskCountType getTaskCountFromSubmissionStatusError(SubmissionStatus submissionStatus);

    TaskCountType taskCount;
    TaskCountType taskLevel;
    FlushStamp flushStamp;

    static constexpr TaskCountType notReady = std::numeric_limits<TaskCountType>::max() - 0xF;
    static constexpr TaskCountType gpuHang = std::numeric_limits<TaskCountType>::max() - 0x5;
    static constexpr TaskCountType outOfDeviceMemory = std::numeric_limits<TaskCountType>::max() - 0x4;
    static constexpr TaskCountType outOfHostMemory = std::numeric_limits<TaskCountType>::max() - 0x3;
};

} // namespace NEO
