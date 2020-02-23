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
    static std::unique_ptr<OSMemory> create();

    virtual ~OSMemory() = default;
    virtual void *reserveCpuAddressRange(size_t sizeToReserve) = 0;
    virtual void releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) = 0;
};

} // namespace NEO
