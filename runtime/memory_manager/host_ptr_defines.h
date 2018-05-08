/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <cstdlib>
#include <cinttypes>

namespace OCLRT {

struct OsHandle;
typedef OsHandle OsGraphicsHandle;

const int max_fragments_count = 3;

enum AllocationType {
    NONE = 0,
    LEADING,
    MIDDLE,
    TRAILING
};

enum OverlapStatus {
    FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER = 0,
    FRAGMENT_WITHIN_STORED_FRAGMENT,
    FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT,
    FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT,
    FRAGMENT_NOT_CHECKED
};

enum RequirementsStatus {
    SUCCESS = 0,
    FATAL
};

struct ResidencyData {
    ResidencyData() {
    }
    bool resident = false;
    uint64_t lastFence = 0;
};

struct PartialAllocation {
    int allocationType = NONE;
    const void *allocationPtr = nullptr;
    size_t allocationSize = 0u;
};

struct AllocationRequirements {
    PartialAllocation AllocationFragments[max_fragments_count];
    uint64_t totalRequiredSize = 0u;
    uint32_t requiredFragmentsCount = 0u;
};

struct FragmentStorage {
    const void *fragmentCpuPointer = nullptr;
    size_t fragmentSize = 0;
    int refCount = 0;
    OsHandle *osInternalStorage = nullptr;
    ResidencyData *residency = nullptr;
    bool driverAllocation = false;
};

struct AllocationStorageData {
    OsHandle *osHandleStorage;
    size_t fragmentSize = 0;
    const void *cpuPtr = nullptr;
    bool freeTheFragment = false;
    ResidencyData *residency = nullptr;
};

struct OsHandleStorage {
    AllocationStorageData fragmentStorageData[max_fragments_count];
    uint32_t fragmentCount = 0;
    OsHandleStorage() {
        for (int i = 0; i < max_fragments_count; i++) {
            fragmentStorageData[i].osHandleStorage = nullptr;
            fragmentStorageData[i].cpuPtr = nullptr;
        }
    }
};

struct CheckedFragments {
    FragmentStorage *fragments[max_fragments_count];
    OverlapStatus status[max_fragments_count];
    size_t count = 0;
};
} // namespace OCLRT
