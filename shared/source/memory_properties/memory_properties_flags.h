/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_properties/memory_properties_flags_common.inl"

namespace NEO {

class Device;

struct MemoryProperties {
    const Device *pDevice = nullptr;
    union {
        MemoryFlags flags;
        uint32_t allFlags = 0;
    };
    union {
        MemoryAllocFlags allocFlags;
        uint32_t allAllocFlags = 0;
    };
    uintptr_t hostptr = 0;
    static_assert(sizeof(MemoryProperties::flags) == sizeof(MemoryProperties::allFlags) && sizeof(MemoryProperties::allocFlags) == sizeof(MemoryProperties::allAllocFlags), "");
};
} // namespace NEO
