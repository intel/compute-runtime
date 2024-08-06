/*
 * Copyright (C) 2018-2024 Intel Corporation
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
    WaitParams() = default;
    WaitParams(bool indefinitelyPoll, bool enableTimeout, bool skipTbxDownload, int64_t waitTimeout)
        : indefinitelyPoll(indefinitelyPoll), enableTimeout(enableTimeout), skipTbxDownload(skipTbxDownload), waitTimeout(waitTimeout){};

    bool indefinitelyPoll = false;
    bool enableTimeout = false;
    bool skipTbxDownload = false;
    int64_t waitTimeout = 0;
};

} // namespace NEO
