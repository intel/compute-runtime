/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"

#include "gtest/gtest.h"

#include <bitset>

TEST(UnifiedMemoryTests, givenInternalMemoryTypesThenAllHaveOnlyOneBitSet) {
    EXPECT_EQ(1u, std::bitset<4>(InternalMemoryType::DEVICE_UNIFIED_MEMORY).count());
    EXPECT_EQ(1u, std::bitset<4>(InternalMemoryType::HOST_UNIFIED_MEMORY).count());
    EXPECT_EQ(1u, std::bitset<4>(InternalMemoryType::SHARED_UNIFIED_MEMORY).count());
    EXPECT_EQ(1u, std::bitset<4>(InternalMemoryType::SVM).count());
}

TEST(UnifiedMemoryTests, givenUnifiedMemoryControlsWhenDedicatedFieldsAreSetThenMaskIsProperlyGenerated) {
    UnifiedMemoryControls controls;
    EXPECT_EQ(0u, controls.generateMask());
    controls.indirectDeviceAllocationsAllowed = true;
    EXPECT_EQ(InternalMemoryType::DEVICE_UNIFIED_MEMORY, controls.generateMask());

    controls.indirectHostAllocationsAllowed = true;
    EXPECT_EQ(InternalMemoryType::HOST_UNIFIED_MEMORY | InternalMemoryType::DEVICE_UNIFIED_MEMORY, controls.generateMask());

    controls.indirectDeviceAllocationsAllowed = false;

    EXPECT_EQ(InternalMemoryType::HOST_UNIFIED_MEMORY, controls.generateMask());

    controls.indirectHostAllocationsAllowed = false;
    controls.indirectSharedAllocationsAllowed = true;
    EXPECT_EQ(InternalMemoryType::SHARED_UNIFIED_MEMORY, controls.generateMask());
}
