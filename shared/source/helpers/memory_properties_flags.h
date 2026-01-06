/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <vector>

namespace NEO {
class Device;

struct MemoryFlags {
    uint32_t readWrite : 1;
    uint32_t writeOnly : 1;
    uint32_t readOnly : 1;
    uint32_t useHostPtr : 1;
    uint32_t allocHostPtr : 1;
    uint32_t copyHostPtr : 1;
    uint32_t hostWriteOnly : 1;
    uint32_t hostReadOnly : 1;
    uint32_t hostNoAccess : 1;
    uint32_t kernelReadAndWrite : 1;
    uint32_t forceLinearStorage : 1;
    uint32_t accessFlagsUnrestricted : 1;
    uint32_t noAccess : 1;
    uint32_t locallyUncachedResource : 1;
    uint32_t locallyUncachedInSurfaceState : 1;
    uint32_t allowUnrestrictedSize : 1;
    uint32_t forceHostMemory : 1;
    uint32_t shareable : 1;
    uint32_t resource48Bit : 1;
    uint32_t compressedHint : 1;
    uint32_t uncompressedHint : 1;
    uint32_t shareableWithoutNTHandle : 1;

    bool operator==(const MemoryFlags &) const = default;
};

struct MemoryAllocFlags {
    uint32_t allocWriteCombined : 1;
    uint32_t usmInitialPlacementCpu : 1;
    uint32_t usmInitialPlacementGpu : 1;
};

struct MemoryProperties {
    uint64_t handle = 0;
    uint64_t handleType = 0;
    uintptr_t hostptr = 0;
    const Device *pDevice = nullptr;
    std::vector<Device *> associatedDevices;
    uint32_t memCacheClos = 0;
    union {
        MemoryFlags flags;
        uint32_t allFlags = 0;
    };
    union {
        MemoryAllocFlags allocFlags;
        uint32_t allAllocFlags = 0;
    };
    static_assert(sizeof(MemoryProperties::flags) == sizeof(MemoryProperties::allFlags) && sizeof(MemoryProperties::allocFlags) == sizeof(MemoryProperties::allAllocFlags), "");
};
} // namespace NEO
