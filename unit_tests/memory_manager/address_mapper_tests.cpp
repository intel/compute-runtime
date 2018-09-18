/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/selectors.h"
#include "runtime/memory_manager/address_mapper.h"
#include "test.h"
#include "gtest/gtest.h"
#include "unit_tests/helpers/memory_management.h"

#include <memory>

using namespace OCLRT;

class AddressMapperFixture {
  public:
    void SetUp() {
        mapper = new AddressMapper();
    }

    void TearDown() {
        delete mapper;
    }
    AddressMapper *mapper;
};

typedef Test<AddressMapperFixture> AddressMapperTests;

TEST_F(AddressMapperTests, mapAlignedPointers) {
    uint32_t m1 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map((void *)0x3000, MemoryConstants::pageSize);
    EXPECT_EQ(0x2000u, m2);
    uint32_t m3 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m3);
}

TEST_F(AddressMapperTests, mapNotAlignedPointers) {
    void *vm1 = (void *)(0x1100);
    void *vm2 = (void *)(0x4100);

    uint32_t m1 = mapper->map(vm1, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map(vm2, MemoryConstants::pageSize);
    EXPECT_EQ(0x3000u, m2);
    uint32_t m3 = mapper->map(vm1, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m3);
}

TEST_F(AddressMapperTests, mapThenResize) {
    uint32_t m1 = mapper->map((void *)0x1000, MemoryConstants::pageSize);
    EXPECT_EQ(0x1000u, m1);
    uint32_t m2 = mapper->map((void *)0x1000, 2 * MemoryConstants::pageSize);
    EXPECT_EQ(0x2000u, m2);
}

TEST_F(AddressMapperTests, unmapNotMapped) {
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
