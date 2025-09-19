/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_physical_address_allocator.h"

using namespace NEO;

template <typename GfxFamily>
class MockPhysicalAddressAllocatorHw : public PhysicalAddressAllocatorHw<GfxFamily> {
  public:
    using PhysicalAddressAllocator::initialPageAddress;
    using PhysicalAddressAllocator::mainAllocator;
    using PhysicalAddressAllocatorHw<GfxFamily>::bankAllocators;
    using PhysicalAddressAllocatorHw<GfxFamily>::memoryBankSize;
    using PhysicalAddressAllocatorHw<GfxFamily>::numberOfBanks;
    using PhysicalAddressAllocatorHw<GfxFamily>::PhysicalAddressAllocatorHw;

    MockPhysicalAddressAllocatorHw() : PhysicalAddressAllocatorHw<GfxFamily>(MemoryConstants::gigaByte, 4) {}
};

using PhysicalAddressAllocatorHwTest = ::testing::Test;

HWTEST_F(PhysicalAddressAllocatorHwTest, givenZeroBanksWhenPageInBankIsReservedThenMainAllocatorIsUsed) {
    size_t bankSize = 1024 * MemoryConstants::pageSize;
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(bankSize, 0);

    auto physAddress = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_NE(0u, physAddress);
    auto address = allocator.mainAllocator.load();

    auto physAddress1 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_NE(0u, physAddress1);
    EXPECT_NE(physAddress, physAddress1);

    auto address1 = allocator.mainAllocator.load();

    EXPECT_NE(address, address1);
}

HWTEST_F(PhysicalAddressAllocatorHwTest, given4BanksWhenAllocatorIsCreatedThen4AllocatorsAreCreated) {
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(2 * MemoryConstants::megaByte, 4);
    EXPECT_NE(nullptr, allocator.bankAllocators);
    EXPECT_EQ(4u, allocator.numberOfBanks);
    EXPECT_EQ(2 * MemoryConstants::megaByte, allocator.memoryBankSize);
}

HWTEST_F(PhysicalAddressAllocatorHwTest, given4BanksWhenReservingFirstPageInBanksThenNonZeroAddressIsReturnedAndEachBankAddressIsDifferent) {
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(2 * MemoryConstants::megaByte, 4);
    auto physAddress0 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_NE(0u, physAddress0);
    auto physAddress1 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_NE(0u, physAddress1);
    auto physAddress2 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(2));
    EXPECT_NE(0u, physAddress2);
    auto physAddress3 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(3));
    EXPECT_NE(0u, physAddress3);

    EXPECT_NE(physAddress0, physAddress1);
    EXPECT_NE(physAddress0, physAddress2);
    EXPECT_NE(physAddress0, physAddress3);
    EXPECT_NE(physAddress1, physAddress2);
    EXPECT_NE(physAddress1, physAddress3);
    EXPECT_NE(physAddress2, physAddress3);
}

HWTEST_F(PhysicalAddressAllocatorHwTest, given4BanksWhenReservingFirstPageInBanksThenEveryNextBankAddressIsOffsetByBankSize) {
    size_t bankSize = 4 * 1024 * MemoryConstants::pageSize;
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(bankSize, 4);
    auto physAddress0 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(0x1000u, physAddress0);

    auto physAddress1 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(bankSize, physAddress1);

    auto physAddress2 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(2));
    EXPECT_EQ(2 * bankSize, physAddress2);

    auto physAddress3 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(3));
    EXPECT_EQ(3 * bankSize, physAddress3);
}

HWTEST_F(PhysicalAddressAllocatorHwTest, givenSpecificBankWhenReservingConsecutivePagesThenReturnedAddressesAreDifferent) {
    auto bankSize = 4 * 1024 * MemoryConstants::pageSize;
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(bankSize, 4);

    uint32_t banks[4] = {MemoryBanks::getBankForLocalMemory(0),
                         MemoryBanks::getBankForLocalMemory(1),
                         MemoryBanks::getBankForLocalMemory(2),
                         MemoryBanks::getBankForLocalMemory(3)};

    for (uint32_t bankIndex = 0; bankIndex < 4; bankIndex++) {
        auto physAddress = allocator.reserve4kPage(banks[bankIndex]);
        EXPECT_NE(0u, physAddress);
        auto physAddress1 = allocator.reserve4kPage(banks[bankIndex]);
        EXPECT_NE(physAddress, physAddress1);
        auto physAddress2 = allocator.reserve4kPage(banks[bankIndex]);
        EXPECT_NE(physAddress1, physAddress2);
    }
}

HWTEST_F(PhysicalAddressAllocatorHwTest, givenSingleBankWhen4kAnd64kPagesAreAllocatedAlternatelyThenCorrectAlignmentIsSatisfied) {
    size_t bankSize = 4 * 1024 * MemoryConstants::pageSize;
    MockPhysicalAddressAllocatorHw<FamilyType> allocator(bankSize, 4);
    auto physAddress0 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_NE(0u, physAddress0);
    EXPECT_EQ(0u, physAddress0 & MemoryConstants::pageMask);

    auto physAddress1 = allocator.reserve64kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_NE(0u, physAddress1);
    EXPECT_EQ(0u, physAddress1 & MemoryConstants::page64kMask);

    auto physAddress2 = allocator.reserve4kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_NE(0u, physAddress2);
    EXPECT_EQ(0u, physAddress2 & MemoryConstants::pageMask);

    auto physAddress3 = allocator.reserve64kPage(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_NE(0u, physAddress3);
    EXPECT_EQ(0u, physAddress3 & MemoryConstants::page64kMask);
}