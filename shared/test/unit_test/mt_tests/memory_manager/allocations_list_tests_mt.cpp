/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocations_list.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace NEO;

TEST(AllocationsListMtTest, givenConcurrentPushTailOneCallsThenAllAllocationsArePreserved) {
    constexpr uint32_t threadCount = 8u;
    constexpr uint32_t pushesPerThread = 32u;

    AllocationsList allocations;
    std::vector<std::unique_ptr<MockGraphicsAllocation>> ownedAllocations;
    ownedAllocations.reserve(threadCount * pushesPerThread);
    for (uint32_t i = 0; i < threadCount * pushesPerThread; i++) {
        ownedAllocations.emplace_back(std::make_unique<MockGraphicsAllocation>());
    }

    std::atomic<bool> startSignal{false};
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (uint32_t threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        threads.emplace_back([&, threadIndex] {
            while (!startSignal.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (uint32_t pushIndex = 0; pushIndex < pushesPerThread; pushIndex++) {
                allocations.pushTailOne(*ownedAllocations[threadIndex * pushesPerThread + pushIndex]);
            }
        });
    }

    startSignal.store(true, std::memory_order_release);
    for (auto &thread : threads) {
        thread.join();
    }

    auto snapshot = allocations.peekAllocations();
    EXPECT_EQ(static_cast<size_t>(threadCount * pushesPerThread), snapshot.size());

    for (auto &owned : ownedAllocations) {
        EXPECT_TRUE(allocations.peekContains(*owned));
    }

    allocations.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}

TEST(AllocationsListMtTest, givenConcurrentPushAndRemoveCallsThenContainerRemainsConsistent) {
    constexpr uint32_t producerCount = 4u;
    constexpr uint32_t consumerCount = 4u;
    constexpr uint32_t pushesPerProducer = 64u;

    AllocationsList allocations;
    std::vector<std::unique_ptr<MockGraphicsAllocation>> ownedAllocations;
    ownedAllocations.reserve(producerCount * pushesPerProducer);
    for (uint32_t i = 0; i < producerCount * pushesPerProducer; i++) {
        ownedAllocations.emplace_back(std::make_unique<MockGraphicsAllocation>());
    }

    std::atomic<uint32_t> nextProducerIndex{0};
    std::atomic<uint32_t> removedCount{0};
    std::atomic<bool> startSignal{false};
    std::vector<std::thread> threads;
    threads.reserve(producerCount + consumerCount);

    for (uint32_t producerId = 0; producerId < producerCount; producerId++) {
        threads.emplace_back([&] {
            while (!startSignal.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            while (true) {
                uint32_t index = nextProducerIndex.fetch_add(1u);
                if (index >= producerCount * pushesPerProducer) {
                    return;
                }
                allocations.pushTailOne(*ownedAllocations[index]);
            }
        });
    }

    for (uint32_t consumerId = 0; consumerId < consumerCount; consumerId++) {
        threads.emplace_back([&] {
            while (!startSignal.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            while (removedCount.load() < producerCount * pushesPerProducer) {
                auto *head = allocations.peekHead();
                if (head == nullptr) {
                    std::this_thread::yield();
                    continue;
                }
                auto removed = allocations.removeOne(*head);
                if (removed != nullptr) {
                    removedCount.fetch_add(1u);
                    removed.release();
                }
            }
        });
    }

    startSignal.store(true, std::memory_order_release);
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(producerCount * pushesPerProducer, removedCount.load());
    EXPECT_TRUE(allocations.peekIsEmpty());
}

TEST(AllocationsListMtTest, givenConcurrentTransferAllAllocationsToCallsThenAllAllocationsArePreservedAcrossLists) {
    constexpr uint32_t threadCount = 6u;
    constexpr uint32_t iterations = 32u;

    AllocationsList sourceList;
    AllocationsList targetList;
    std::vector<std::unique_ptr<MockGraphicsAllocation>> ownedAllocations;
    ownedAllocations.reserve(threadCount * iterations);
    for (uint32_t i = 0; i < threadCount * iterations; i++) {
        ownedAllocations.emplace_back(std::make_unique<MockGraphicsAllocation>());
        sourceList.pushTailOne(*ownedAllocations.back());
    }

    std::atomic<bool> startSignal{false};
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (uint32_t threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        threads.emplace_back([&] {
            while (!startSignal.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (uint32_t iteration = 0; iteration < iterations; iteration++) {
                sourceList.transferAllAllocationsTo(targetList);
                targetList.transferAllAllocationsTo(sourceList);
            }
        });
    }

    startSignal.store(true, std::memory_order_release);
    for (auto &thread : threads) {
        thread.join();
    }

    const auto totalCount = static_cast<size_t>(threadCount * iterations);
    const auto sourceSize = sourceList.peekAllocations().size();
    const auto targetSize = targetList.peekAllocations().size();
    EXPECT_EQ(totalCount, sourceSize + targetSize);

    for (auto &owned : ownedAllocations) {
        EXPECT_TRUE(sourceList.peekContains(*owned) || targetList.peekContains(*owned));
    }

    sourceList.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
    targetList.removeMatching([](GraphicsAllocation *) { return true; }, [](GraphicsAllocation *) {});
}
