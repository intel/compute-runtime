/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class WaitStatus {
    NotReady = 0,
    Ready = 1,
    GpuHang = 2,
};

struct WaitParams {
    bool indefinitelyPoll = false;
    bool enableTimeout = false;
    int64_t waitTimeout = 0;
};

} // namespace NEO