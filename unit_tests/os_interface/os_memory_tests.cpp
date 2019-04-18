/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_memory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSMemory, reserveCpuAddressRange) {
    size_t reservedCpuAddressRangeSize = 1024;
    auto reservedCpuAddressRange = OSMemory::reserveCpuAddressRange(reservedCpuAddressRangeSize);
    EXPECT_NE(reservedCpuAddressRange, nullptr);
    OSMemory::releaseCpuAddressRange(reservedCpuAddressRange, reservedCpuAddressRangeSize);
}
