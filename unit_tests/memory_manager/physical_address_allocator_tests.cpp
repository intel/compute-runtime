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
    auto physAddress = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingConsecutive4kPagesThenReturnedAddressesAreDifferentAndAligned) {
    MockPhysicalAddressAllocator allocator;

    auto physAddress = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
    EXPECT_EQ(0u, physAddress & MemoryConstants::pageMask);

    auto physAddress1 = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress, physAddress1);
    EXPECT_EQ(0u, physAddress1 & MemoryConstants::pageMask);

    auto physAddress2 = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress1, physAddress2);
    EXPECT_EQ(0u, physAddress2 & MemoryConstants::pageMask);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingFirst64kPageThen64kAlignedIsReturned) {
    MockPhysicalAddressAllocator allocator;
    auto physAddress = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
    EXPECT_EQ(0u, physAddress & MemoryConstants::page64kMask);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingConsecutive64kPagesThenReturnedAddressesAreDifferentAndAligned) {
    MockPhysicalAddressAllocator allocator;

    auto physAddress = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
    EXPECT_EQ(0u, physAddress & MemoryConstants::page64kMask);

    auto physAddress1 = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress, physAddress1);
    EXPECT_EQ(0u, physAddress & MemoryConstants::page64kMask);

    auto physAddress2 = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress1, physAddress2);
    EXPECT_EQ(0u, physAddress & MemoryConstants::page64kMask);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingInterleaving4kPagesAnd64kPagesThenReturnedAddressesAreCorrectlyAligned) {
    MockPhysicalAddressAllocator allocator;

    auto physAddress = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(0u, physAddress);
    EXPECT_EQ(0u, physAddress & MemoryConstants::pageMask);

    auto physAddress1 = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress, physAddress1);
    EXPECT_EQ(0u, physAddress1 & MemoryConstants::page64kMask);

    auto physAddress2 = allocator.reserve4kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress1, physAddress2);
    EXPECT_EQ(0u, physAddress2 & MemoryConstants::pageMask);

    auto physAddress3 = allocator.reserve64kPage(MemoryBanks::MainBank);
    EXPECT_NE(physAddress, physAddress1);
    EXPECT_EQ(0u, physAddress3 & MemoryConstants::page64kMask);
}
