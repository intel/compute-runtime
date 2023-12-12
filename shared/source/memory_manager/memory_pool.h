/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class MemoryPool {
    memoryNull,
    system4KBPages,
    system64KBPages,
    system4KBPagesWith32BitGpuAddressing,
    system64KBPagesWith32BitGpuAddressing,
    systemCpuInaccessible,
    localMemory,
};

namespace MemoryPoolHelper {

template <typename... Args>
inline bool isSystemMemoryPool(Args... pool) {
    return ((pool == MemoryPool::system4KBPages ||
             pool == MemoryPool::system64KBPages ||
             pool == MemoryPool::system4KBPagesWith32BitGpuAddressing ||
             pool == MemoryPool::system64KBPagesWith32BitGpuAddressing) &&
            ...);
}

} // namespace MemoryPoolHelper
} // namespace NEO
