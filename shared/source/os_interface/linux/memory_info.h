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
class Drm;

class MemoryInfo {
  public:
    MemoryInfo() = default;
    virtual ~MemoryInfo() = 0;
    virtual size_t getMemoryRegionSize(uint32_t memoryBank) = 0;
    virtual uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) = 0;
    virtual uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) = 0;
};

inline MemoryInfo::~MemoryInfo(){};

} // namespace NEO
