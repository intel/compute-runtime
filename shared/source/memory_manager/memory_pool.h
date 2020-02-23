/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/extendable_enum.h"

namespace MemoryPool {
struct Type : ExtendableEnum {
    constexpr Type(uint32_t val) : ExtendableEnum(val) {}
};
constexpr Type MemoryNull{0};
constexpr Type System4KBPages{1};
constexpr Type System64KBPages{2};
constexpr Type System4KBPagesWith32BitGpuAddressing{3};
constexpr Type System64KBPagesWith32BitGpuAddressing{4};
constexpr Type SystemCpuInaccessible{5};
constexpr Type LocalMemory{6};

inline bool isSystemMemoryPool(Type pool) {
    return pool == System4KBPages ||
           pool == System64KBPages ||
           pool == System4KBPagesWith32BitGpuAddressing ||
           pool == System64KBPagesWith32BitGpuAddressing;
}
} // namespace MemoryPool
