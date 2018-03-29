/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
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
