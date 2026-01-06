/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/pool_info.h"

namespace NEO {
static constexpr uint64_t KB = MemoryConstants::kiloByte; // NOLINT(readability-identifier-naming)

// clang-format off
const std::array<const PoolInfo, 3> PoolInfo::poolInfos = {
    PoolInfo{ 0,          256,    2 * KB},
    PoolInfo{ 256 + 1,    1 * KB, 8 * KB},
    PoolInfo{ 1 * KB + 1, MemoryConstants::pageSize, 2 * MemoryConstants::pageSize}};
// clang-format on

const std::array<const PoolInfo, 3> PoolInfo::extendedPoolInfos = PoolInfo::poolInfos;

const std::array<const PoolInfo, 3> PoolInfo::getPoolInfos(const GfxCoreHelper &gfxCoreHelper) {
    return poolInfos;
}

const std::array<const PoolInfo, 3> PoolInfo::getHostPoolInfos() {
    return poolInfos;
}

size_t PoolInfo::getMaxPoolableSize(const GfxCoreHelper &gfxCoreHelper) {
    return MemoryConstants::pageSize;
}

size_t PoolInfo::getHostMaxPoolableSize() {
    return MemoryConstants::pageSize;
}
} // namespace NEO