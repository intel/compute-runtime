/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>
#include <linux/limits.h>
#include <unordered_map>
#include <vector>

namespace NEO {

struct IsaDebugData {

    uint64_t totalSegments;
    char elfPath[PATH_MAX];
    struct DebugDataBindInfo {
        uint64_t address;
        uint64_t size;
    };
    using vmHandle = uint64_t;
    std::unordered_map<vmHandle, std::vector<DebugDataBindInfo>> bindInfoMap;
    inline static std::atomic<uint32_t> nextDebugDataMapHandle = 0;
};

} // namespace NEO
