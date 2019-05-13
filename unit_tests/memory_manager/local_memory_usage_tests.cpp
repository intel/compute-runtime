/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/basic_math.h"
#include "runtime/memory_manager/local_memory_usage.h"

#include "third_party/gtest/gtest/gtest.h"

namespace NEO {

struct MockLocalMemoryUsageBankSelector : public LocalMemoryUsageBankSelector {
    using LocalMemoryUsageBankSelector::banksCount;
    using LocalMemoryUsageBankSelector::LocalMemoryUsageBankSelector;
    std::atomic<uint64_t> *getMemorySizes() { return memorySizes.get(); }
};

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenItsCreatedAllValuesAreZero) {
    MockLocalMemoryUsageBankSelector selector(2u);

    for (uint32_t i = 0; i < selector.banksCount; i++) {
        EXPECT_EQ(0u, selector.getMemorySizes()[i].load());
    }
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMemoryIsReservedOnGivenBankThenValueStoredInTheArrayIsCorrect) {
    LocalMemoryUsageBankSelector selector(4u);

    uint64_t allocationSize = 1024u;
    auto bankIndex = selector.getLeastOccupiedBank();
    selector.reserveOnBank(bankIndex, allocationSize);

    EXPECT_EQ(allocationSize, selector.getOccupiedMemorySizeForBank(bankIndex));
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenMemoryIsReleasedThenValueIsCorrectlyAllocated) {
    LocalMemoryUsageBankSelector selector(1u);

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
    LocalMemoryUsageBankSelector selector(3u);
    EXPECT_THROW(selector.reserveOnBank(8u, 1024u), std::exception);
    EXPECT_THROW(selector.freeOnBank(8u, 1024u), std::exception);
    EXPECT_THROW(selector.getOccupiedMemorySizeForBank(8u), std::exception);
}

TEST(localMemoryUsageTest, givenLocalMemoryUsageBankSelectorWhenItsCreatedWithZeroBanksThenErrorIsReturned) {
    EXPECT_THROW(LocalMemoryUsageBankSelector(0u), std::exception);
}
} // namespace NEO
