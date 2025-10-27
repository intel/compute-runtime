/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/pool_info.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {
static constexpr uint64_t KB = MemoryConstants::kiloByte; // NOLINT(readability-identifier-naming)
static constexpr uint64_t MB = MemoryConstants::megaByte; // NOLINT(readability-identifier-naming)
// clang-format off
const std::array<const PoolInfo, 3> PoolInfo::poolInfos = {
    PoolInfo{ 0,           4 * KB, 2 * MB},
    PoolInfo{ 4 * KB + 1, 64 * KB, 2 * MB},
    PoolInfo{64 * KB + 1,  1 * MB, 2 * MB}};

const std::array<const PoolInfo, 3> PoolInfo::extendedPoolInfos = {
    PoolInfo{ 0,           4 * KB,  2 * MB},
    PoolInfo{ 4 * KB + 1, 64 * KB,  2 * MB},
    PoolInfo{64 * KB + 1,  2 * MB, 16 * MB}};
// clang-format on

const std::array<const PoolInfo, 3> PoolInfo::getPoolInfos(const GfxCoreHelper &gfxCoreHelper) {
    if (gfxCoreHelper.isExtendedUsmPoolSizeEnabled()) {
        return extendedPoolInfos;
    }
    return poolInfos;
}

size_t PoolInfo::getMaxPoolableSize(const GfxCoreHelper &gfxCoreHelper) {
    if (gfxCoreHelper.isExtendedUsmPoolSizeEnabled()) {
        return 2 * MB;
    }
    return 1 * MB;
}
} // namespace NEO
