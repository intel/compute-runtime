/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
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
    uint32_t forceSharedPhysicalMemory : 1;
};

struct MemoryAllocFlags {
    uint32_t allocWriteCombined : 1;
    uint32_t allocDefault : 1;
};

} // namespace NEO
