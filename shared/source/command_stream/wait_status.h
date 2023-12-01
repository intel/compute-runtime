/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class WaitStatus {
    notReady = 0,
    ready = 1,
    gpuHang = 2,
};

struct WaitParams {
    bool indefinitelyPoll = false;
    bool enableTimeout = false;
    int64_t waitTimeout = 0;
};

} // namespace NEO
