/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class MemoryPool {
    MemoryNull,
    System4KBPages,
    System64KBPages,
    System4KBPagesWith32BitGpuAddressing,
    System64KBPagesWith32BitGpuAddressing,
    SystemCpuInaccessible,
    LocalMemory,
};

namespace MemoryPoolHelper {

inline bool isSystemMemoryPool(MemoryPool pool) {
    return pool == MemoryPool::System4KBPages ||
           pool == MemoryPool::System64KBPages ||
           pool == MemoryPool::System4KBPagesWith32BitGpuAddressing ||
           pool == MemoryPool::System64KBPagesWith32BitGpuAddressing;
}

} // namespace MemoryPoolHelper
} // namespace NEO
