/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <array>
#include <cstddef>

namespace NEO {
class PoolInfo {
  public:
    size_t minServicedSize;
    size_t maxServicedSize;
    size_t poolSize;
    bool operator<(const PoolInfo &rhs) const {
        return this->minServicedSize < rhs.minServicedSize;
    }

    static const std::array<const PoolInfo, 3> getPoolInfos() {
        return poolInfos;
    }

    static size_t getMaxPoolableSize();

  private:
    static const std::array<const PoolInfo, 3> poolInfos;
};
} // namespace NEO
