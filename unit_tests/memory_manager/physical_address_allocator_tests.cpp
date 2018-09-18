/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/page_table.h"
#include "unit_tests/mocks/mock_physical_address_allocator.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingFirstPageThenNonZeroAddressIsReturned) {
    MockPhysicalAddressAllocator allocator;
    auto physAddress = allocator.reservePage(PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(0u, physAddress);
}

TEST(PhysicalAddressAllocator, givenPhysicalAddressesAllocatorWhenReservingConsecutivePagesThenReturnedAddressesAreDifferent) {
    MockPhysicalAddressAllocator allocator;

    auto physAddress = allocator.reservePage(PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(0u, physAddress);
    auto physAddress1 = allocator.reservePage(PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(physAddress, physAddress1);
    auto physAddress2 = allocator.reservePage(PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(physAddress1, physAddress2);
}
