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

enum class CacheRegion : uint16_t {
    None = 0,
    Region1,
    Region2,
    Default = None
};

struct CacheInfo {
    CacheInfo() = default;
    virtual ~CacheInfo() = 0;

    virtual bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) = 0;
};

inline CacheInfo::~CacheInfo(){};

} // namespace NEO
