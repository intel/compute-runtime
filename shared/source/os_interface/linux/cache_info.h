/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace NEO {

enum class CachePolicy : uint32_t {
    Uncached = 0,
    WriteCombined = 1,
    WriteThrough = 2,
    WriteBack = 3,
};

enum class CacheRegion : uint16_t {
    Default = 0,
    Region1,
    Region2,
    Count,
    None = 0xFFFF
};

struct CacheInfo {
    CacheInfo() = default;
    virtual ~CacheInfo() = 0;

    virtual bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) = 0;
};

inline CacheInfo::~CacheInfo(){};

} // namespace NEO
