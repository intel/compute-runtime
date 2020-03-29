/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <memory>

namespace NEO {

struct OSMemory {
  public:
    struct ReservedCpuAddressRange {
        void *originalPtr = nullptr;
        void *alignedPtr = nullptr;
        size_t actualReservedSize = 0;
    };

  public:
    static std::unique_ptr<OSMemory> create();

    virtual ~OSMemory() = default;

    MOCKABLE_VIRTUAL ReservedCpuAddressRange reserveCpuAddressRange(size_t sizeToReserve, size_t alignment);
    MOCKABLE_VIRTUAL void releaseCpuAddressRange(const ReservedCpuAddressRange &reservedCpuAddressRange);

  protected:
    virtual void *osReserveCpuAddressRange(size_t sizeToReserve) = 0;
    virtual void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) = 0;
};

} // namespace NEO
