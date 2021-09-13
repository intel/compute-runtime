/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

class MemoryInfo {
  public:
    MemoryInfo() = default;
    virtual ~MemoryInfo() = 0;
    virtual size_t getMemoryRegionSize(uint32_t memoryBank) = 0;
};

inline MemoryInfo::~MemoryInfo(){};

} // namespace NEO
