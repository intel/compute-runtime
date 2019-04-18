/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>

namespace NEO {

struct OSMemory {
  public:
    static void *reserveCpuAddressRange(size_t sizeToReserve);
    static void releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize);
};

} // namespace NEO
