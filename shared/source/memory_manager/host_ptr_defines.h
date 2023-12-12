/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cinttypes>
#include <cstddef>
#include <limits>

namespace NEO {

struct OsHandle {
    virtual ~OsHandle() = default;

  protected:
    OsHandle() = default;
};
struct ResidencyData;

constexpr int maxFragmentsCount = 3;

enum class FragmentPosition {
    none = 0,
    leading,
    middle,
    trailing
};

enum class RequirementsStatus {
    success = 0,
    fatal
};

struct PartialAllocation {
    FragmentPosition fragmentPosition = FragmentPosition::none;
    const void *allocationPtr = nullptr;
    size_t allocationSize = 0u;
};

struct AllocationRequirements {
    PartialAllocation allocationFragments[maxFragmentsCount];
    uint64_t totalRequiredSize = 0u;
    uint32_t requiredFragmentsCount = 0u;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
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
