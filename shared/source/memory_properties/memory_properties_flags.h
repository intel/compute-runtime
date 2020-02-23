/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_properties/memory_properties_flags_common.inl"

namespace NEO {

struct MemoryPropertiesFlags {
    union {
        MemoryFlags flags;
        uint32_t allFlags = 0;
    };
    union {
        MemoryAllocFlags allocFlags;
        uint32_t allAllocFlags = 0;
    };
    static_assert(sizeof(MemoryPropertiesFlags::flags) == sizeof(MemoryPropertiesFlags::allFlags) && sizeof(MemoryPropertiesFlags::allocFlags) == sizeof(MemoryPropertiesFlags::allAllocFlags), "");
};
} // namespace NEO
