/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <chrono>
#include <cstdint>

namespace L0 {
namespace Sysman {

struct SteadyClock {
    typedef std::chrono::duration<uint64_t, std::milli> duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::time_point<SteadyClock> time_point;
    static time_point now() noexcept {
        static auto epoch = std::chrono::steady_clock::now();
        return time_point(std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - epoch));
    }
};

} // namespace Sysman
} // namespace L0
