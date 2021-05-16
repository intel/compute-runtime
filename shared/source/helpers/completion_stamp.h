/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
typedef uint64_t FlushStamp;
struct CompletionStamp {
    uint32_t taskCount;
    uint32_t taskLevel;
    FlushStamp flushStamp;

    static const uint32_t notReady;
};

} // namespace NEO
