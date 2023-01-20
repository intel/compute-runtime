/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/address_mapper.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class AddressMapperFixture {
  public:
    void setUp() {
        mapper = new AddressMapper();
    }

    void tearDown() {
        delete mapper;
    }
    AddressMapper *mapper;
};

using AddressMapperTests = Test<AddressMapperFixture>;

TEST_F(AddressMapperTests, GivenAlignedPointersWhenMappingThenPointersAreAligned) {
    uint32_t m1 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map((void *)0x3000, MemoryConstants::pageSize);
    EXPECT_EQ(0x2000u, m2);
    uint32_t m3 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m3);
}

TEST_F(AddressMapperTests, GivenUnalignedPointersWhenMappingThenPointersAreAligned) {
    void *vm1 = (void *)(0x1100);
    void *vm2 = (void *)(0x4100);

    uint32_t m1 = mapper->map(vm1, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map(vm2, MemoryConstants::pageSize);
    EXPECT_EQ(0x3000u, m2);
    uint32_t m3 = mapper->map(vm1, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m3);
}

TEST_F(AddressMapperTests, WhenResizingThenPointerIsAligned) {
    uint32_t m1 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map((void *)0x1000, 2 * MemoryConstants::pageSize);
    EXPECT_EQ(0x2000u, m2);
}

TEST_F(AddressMapperTests, WhenUnmappingThenMappingWorksCorrectly) {
    mapper->unmap((void *)0x1000);
    uint32_t m1 = mapper->map((void *)0x2000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);

    // no crash expected
    mapper->unmap((void *)0x1000);

    mapper->unmap((void *)0x2000);
    uint32_t m2 = mapper->map((void *)0x2000, MemoryConstants::pageSize);
    EXPECT_NE(m1, m2);
    EXPECT_EQ(0x2000u, m2);
}
