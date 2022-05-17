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
struct CompletionStamp {
    uint32_t taskCount;
    uint32_t taskLevel;
    FlushStamp flushStamp;

    static constexpr uint32_t notReady = 0xFFFFFFF0;
    static constexpr uint32_t gpuHang = 0xFFFFFFFA;
};

} // namespace NEO
