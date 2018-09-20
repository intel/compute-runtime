/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/page_table.h"
#include "unit_tests/mocks/mock_physical_address_allocator.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingFirstPageThenNonZeroAddressIsReturned) {
    MockPhysicalAddressAllocator allocator;
    auto physAddress = allocator.reservePage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingConsecutivePagesThenReturnedAddressesAreDifferent) {
    MockPhysicalAddressAllocator allocator;

    auto physAddress = allocator.reservePage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
    auto physAddress1 = allocator.reservePage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress, physAddress1);
    auto physAddress2 = allocator.reservePage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress1, physAddress2);
}
