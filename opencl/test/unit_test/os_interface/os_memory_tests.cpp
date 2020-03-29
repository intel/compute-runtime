/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/os_memory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSMemory, reserveCpuAddressRange) {
    auto osMemory = OSMemory::create();
    size_t reservedCpuAddressRangeSize = 1024;
    auto reservedCpuAddressRange = osMemory->reserveCpuAddressRange(reservedCpuAddressRangeSize, MemoryConstants::pageSize64k);
    EXPECT_NE(reservedCpuAddressRange.originalPtr, nullptr);
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(reservedCpuAddressRange.alignedPtr));
    osMemory->releaseCpuAddressRange(reservedCpuAddressRange);
}
