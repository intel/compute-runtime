/*
 * Copyright (c) 2018, Intel Corporation
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
