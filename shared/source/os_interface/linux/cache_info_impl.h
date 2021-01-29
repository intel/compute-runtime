/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/cache_info.h"

namespace NEO {

struct CacheInfoImpl : public CacheInfo {
    CacheInfoImpl() {}

    ~CacheInfoImpl() override = default;

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override { return false; }
};

} // namespace NEO
