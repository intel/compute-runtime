/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/task_count_helper.h"

#include <cstdint>

namespace NEO {
typedef uint64_t FlushStamp;
struct CompletionStamp {
    TaskCountType taskCount;
    TaskCountType taskLevel;
    FlushStamp flushStamp;
};
} // namespace NEO
