/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"

#include "gtest/gtest.h"

#include <bitset>

TEST(UnifiedMemoryTests, givenInternalMemoryTypesThenAllHaveOnlyOneBitSet) {
    EXPECT_EQ(1u, std::bitset<4>(static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory)).count());
    EXPECT_EQ(1u, std::bitset<4>(static_cast<uint32_t>(InternalMemoryType::hostUnifiedMemory)).count());
    EXPECT_EQ(1u, std::bitset<4>(static_cast<uint32_t>(InternalMemoryType::sharedUnifiedMemory)).count());
    EXPECT_EQ(1u, std::bitset<4>(static_cast<uint32_t>(InternalMemoryType::svm)).count());
}

TEST(UnifiedMemoryTests, givenUnifiedMemoryControlsWhenDedicatedFieldsAreSetThenMaskIsProperlyGenerated) {
    UnifiedMemoryControls controls;
    EXPECT_EQ(0u, controls.generateMask());
    controls.indirectDeviceAllocationsAllowed = true;
    EXPECT_EQ(static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory), controls.generateMask());

    controls.indirectHostAllocationsAllowed = true;
    EXPECT_EQ(static_cast<uint32_t>(InternalMemoryType::hostUnifiedMemory) | static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory), controls.generateMask());

    controls.indirectDeviceAllocationsAllowed = false;

    EXPECT_EQ(static_cast<uint32_t>(InternalMemoryType::hostUnifiedMemory), controls.generateMask());

    controls.indirectHostAllocationsAllowed = false;
    controls.indirectSharedAllocationsAllowed = true;
    EXPECT_EQ(static_cast<uint32_t>(InternalMemoryType::sharedUnifiedMemory), controls.generateMask());
}
