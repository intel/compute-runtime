/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_memory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSMemory, reserveCpuAddressRange) {
    auto osMemory = OSMemory::create();
    size_t reservedCpuAddressRangeSize = 1024;
    auto reservedCpuAddressRange = osMemory->reserveCpuAddressRange(reservedCpuAddressRangeSize);
    EXPECT_NE(reservedCpuAddressRange, nullptr);
    osMemory->releaseCpuAddressRange(reservedCpuAddressRange, reservedCpuAddressRangeSize);
}
