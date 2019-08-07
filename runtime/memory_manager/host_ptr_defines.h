/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/residency.h"

#include <cinttypes>
#include <cstdlib>

namespace NEO {

struct OsHandle;

using OsGraphicsHandle = OsHandle;

constexpr int maxFragmentsCount = 3;

enum class FragmentPosition {
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

struct PartialAllocation {
    FragmentPosition fragmentPosition = FragmentPosition::NONE;
    const void *allocationPtr = nullptr;
    size_t allocationSize = 0u;
};

struct AllocationRequirements {
    PartialAllocation allocationFragments[maxFragmentsCount];
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
    OsHandle *osHandleStorage = nullptr;
    size_t fragmentSize = 0;
    const void *cpuPtr = nullptr;
    bool freeTheFragment = false;
    ResidencyData *residency = nullptr;
};

struct OsHandleStorage {
    AllocationStorageData fragmentStorageData[maxFragmentsCount];
    uint32_t fragmentCount = 0;
};

} // namespace NEO
