/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/pool_info.h"

#include "shared/source/helpers/constants.h"

namespace NEO {
static constexpr uint64_t KB = MemoryConstants::kiloByte; // NOLINT(readability-identifier-naming)
static constexpr uint64_t MB = MemoryConstants::megaByte; // NOLINT(readability-identifier-naming)
// clang-format off
const std::array<const PoolInfo, 3> PoolInfo::poolInfos = {
    PoolInfo{ 0,          4 * KB,  2 * MB},
    PoolInfo{ 4 * KB+1,  64 * KB,  2 * MB},
    PoolInfo{64 * KB+1,   2 * MB, 16 * MB}};
// clang-format on

size_t PoolInfo::getMaxPoolableSize() {
    return 2 * MB;
}

} // namespace NEO
