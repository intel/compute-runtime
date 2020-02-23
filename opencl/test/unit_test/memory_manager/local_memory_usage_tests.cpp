/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/local_memory_usage.h"

#include "third_party/gtest/gtest/gtest.h"

namespace NEO {

struct MockLocalMemoryUsageBankSelector : public LocalMemoryUsageBankSelector {
    using LocalMemoryUsageBankSelector::banksCount;
    using LocalMemoryUsageBankSelector::freeOnBank;
    using LocalMemoryUsageBankSelector::LocalMemoryUsageBankSelector;
    using LocalMemoryUsageBankSelector::reserveOnBank;
    using LocalMemoryUsageBankSelector::updateUsageInfo;
    std::atomic<uint64_t> *getMemorySizes() { return memorySizes.get(); }
};

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenItsCreatedAllValuesAreZero) {
    MockLocalMemoryUsageBankSelector selector(2u);

    for (uint32_t i = 0; i < selector.banksCount; i++) {
        EXPECT_EQ(0u, selector.getMemorySizes()[i].load());
    }
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMemoryIsReservedOnGivenBankThenValueStoredInTheArrayIsCorrect) {
    MockLocalMemoryUsageBankSelector selector(4u);

    uint64_t allocationSize = 1024u;
    auto bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);

    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(bankIndex));
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMemoryIsReleasedThenValueIsCorrectlyAllocated) {
    MockLocalMemoryUsageBankSelector selector(1u);

    uint64_t allocationSize = 1024u;
    auto bankIndex = selector.getLeastOccupiedBank();
    EXPECT_EQ(0u, bankIndex);
    selector.reserveOnBank(bankIndex, allocationSize);

    bankIndex = selector.getLeastOccupiedBank();
    EXPECT_EQ(0u, bankIndex);
    selector.reserveOnBank(bankIndex, allocationSize);

    selector.freeOnBank(bankIndex, allocationSize);

    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(bankIndex));
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMemoryAllocatedSeveralTimesItIsStoredOnDifferentBanks) {
    MockLocalMemoryUsageBankSelector selector(5u);

    uint64_t allocationSize = 1024u;

    auto bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);
    bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);
    bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);
    bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);
    bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);

    for (uint32_t i = 0; i < selector.banksCount; i++) {
        EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(i));
    }
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenIndexIsInvalidThenErrorIsReturned) {
    MockLocalMemoryUsageBankSelector selector(3u);
    EXPECT_THROW(selector.reserveOnBank(8u, 1024u), std::exception);
    EXPECT_THROW(selector.freeOnBank(8u, 1024u), std::exception);
    EXPECT_THROW(selector.getOccupiedMemorySizeForBank(8u), std::exception);
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenItsCreatedWithZeroBanksThenErrorIsReturned) {
    EXPECT_THROW(LocalMemoryUsageBankSelector(0u), std::exception);
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMultipleBanksAreUsedThenMemoryIsReservedOnEachOfThem) {
    MockLocalMemoryUsageBankSelector selector(6u);
    uint32_t banks = 5u;
    uint64_t allocationSize = 1024u;

    selector.reserveOnBanks(banks, allocationSize);
    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(0u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(1u));
    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(2u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(3u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(4u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(5u));
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMultipleBanksAreUsedThenMemoryIsReleasedOnEachOfThem) {
    MockLocalMemoryUsageBankSelector selector(6u);
    uint32_t banks = 5u;
    uint64_t allocationSize = 1024u;

    selector.reserveOnBanks(banks, allocationSize);
    selector.reserveOnBanks(banks, allocationSize);

    EXPECT_EQ(2 * allocationSize, selector.getOccupiedMemorySizeForBank(0u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(1u));
    EXPECT_EQ(2 * allocationSize, selector.getOccupiedMemorySizeForBank(2u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(3));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(4));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(5));

    selector.freeOnBanks(banks, allocationSize);
    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(0u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(1u));
    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(2u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(3u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(4u));
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(5u));
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenThereAreMoreThan32BanksThenOnly32AreUpdated) {
    MockLocalMemoryUsageBankSelector selector(33u);
    uint32_t banks = ~0u;
    uint64_t allocationSize = 1024u;

    selector.reserveOnBanks(banks, allocationSize);
    EXPECT_EQ(0u, selector.getOccupiedMemorySizeForBank(32));
}

} // namespace NEO
