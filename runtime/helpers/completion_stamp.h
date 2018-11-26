/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "engine_node.h"
#include <cstdint>

namespace OCLRT {
typedef uint64_t FlushStamp;
struct CompletionStamp {
    uint32_t taskCount;
    uint32_t taskLevel;
    FlushStamp flushStamp;
};
} // namespace OCLRT
