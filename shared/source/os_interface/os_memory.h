/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {

struct OSMemory {
  public:
    struct ReservedCpuAddressRange {
        void *originalPtr = nullptr;
        void *alignedPtr = nullptr;
        size_t sizeToReserve = 0;
        size_t actualReservedSize = 0;
    };

    struct MappedRegion {
        uint64_t start = 0, end = 0;
    };

    using MemoryMaps = std::vector<OSMemory::MappedRegion>;

  public:
    static std::unique_ptr<OSMemory> create();

    virtual ~OSMemory() = default;

    MOCKABLE_VIRTUAL ReservedCpuAddressRange reserveCpuAddressRange(size_t sizeToReserve, size_t alignment);
    MOCKABLE_VIRTUAL ReservedCpuAddressRange reserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, size_t alignment);

    MOCKABLE_VIRTUAL void releaseCpuAddressRange(const ReservedCpuAddressRange &reservedCpuAddressRange);
    virtual void getMemoryMaps(MemoryMaps &memoryMaps) = 0;

    virtual void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) = 0;
    virtual void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) = 0;
};

} // namespace NEO
