/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"

#include "gtest/gtest.h"

#include <array>
#include <memory>
#include <vector>

struct DummyBufferPool;

struct DummyBuffer {
    DummyBuffer(int v) : val{v} {}
    int val;
};

struct DummyBuffersPool : public NEO::AbstractBuffersPool<DummyBuffersPool, DummyBuffer> {
    using BaseType = NEO::AbstractBuffersPool<DummyBuffersPool, DummyBuffer>;
    static constexpr auto dummyPtr = 0xdeadbeef0000;

    static constexpr NEO::SmallBuffersParams defaultParams{
        .aggregatedSmallBuffersPoolSize = 32 * MemoryConstants::kiloByte,
        .smallBufferThreshold = 2 * MemoryConstants::kiloByte,
        .chunkAlignment = 1024u,
        .startingOffset = 1024u};

    DummyBuffersPool(NEO::MemoryManager *memoryManager, uint32_t poolOffset, BaseType::OnChunkFreeCallback onChunkFreeCallback)
        : BaseType{memoryManager, onChunkFreeCallback, defaultParams} {
        dummyAllocations.resize(2);
        dummyAllocations[0] = reinterpret_cast<NEO::GraphicsAllocation *>(poolOffset + dummyPtr);
        dummyAllocations[1] = nullptr; // makes sure nullptrs don't cause SEGFAULTs
    }

    DummyBuffersPool(NEO::MemoryManager *memoryManager) : DummyBuffersPool(memoryManager, 0x0, &DummyBuffersPool::onChunkFree) {}

    BaseType::AllocsVecCRef getAllocationsVector() {
        return dummyAllocations;
    }
    void onChunkFree(uint64_t offset, size_t size) {
        this->freedChunks.push_back({offset, size});
        this->onChunkFreeCalled = true;
    }

    StackVec<NEO::GraphicsAllocation *, 1> dummyAllocations;
    std::vector<std::pair<uint64_t, size_t>> freedChunks{};
    bool onChunkFreeCalled = false;
};

struct DummyBuffersAllocator : public NEO::AbstractBuffersAllocator<DummyBuffersPool, DummyBuffer> {
    using BaseType = NEO::AbstractBuffersAllocator<DummyBuffersPool, DummyBuffer>;
    using BaseType::addNewBufferPool;
    using BaseType::bufferPools;
    using BaseType::isSizeWithinThreshold;

    DummyBuffersAllocator() : BaseType() {}
    DummyBuffersAllocator(const NEO::SmallBuffersParams &params) : BaseType(params) {}

    void drainUnderLock() {
        auto lock = std::unique_lock<std::mutex>(this->mutex);
        this->BaseType::drain();
    }
};

using NEO::MockExecutionEnvironment;
using NEO::MockMemoryManager;

struct AbstractSmallBuffersTest : public ::testing::Test {
    void SetUp() override {
        this->memoryManager.reset(new MockMemoryManager{this->executionEnvironment});
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    static constexpr auto testVal = 0x1234;
};

TEST_F(AbstractSmallBuffersTest, givenBuffersPoolWhenCreatedAndMovedThenCtorsWorkCorrectly) {
    auto pool1 = DummyBuffersPool{this->memoryManager.get()};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    pool1.chunkAllocator.reset(new NEO::HeapAllocator{DummyBuffersPool::defaultParams.startingOffset,
                                                      DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize,
                                                      DummyBuffersPool::defaultParams.chunkAlignment,
                                                      DummyBuffersPool::defaultParams.smallBufferThreshold});

    EXPECT_EQ(pool1.memoryManager, this->memoryManager.get());

    auto pool2 = std::move(pool1);
    EXPECT_EQ(pool2.memoryManager, this->memoryManager.get());
    EXPECT_EQ(pool2.mainStorage->val, testVal);
    EXPECT_EQ(static_cast<DummyBuffersPool::BaseType &>(pool2).getAllocationsVector()[0], reinterpret_cast<NEO::GraphicsAllocation *>(DummyBuffersPool::dummyPtr));
    EXPECT_EQ(pool2.chunkAllocator->getUsedSize(), 0ul);
    EXPECT_EQ(pool2.chunkAllocator->getLeftSize(), DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenPoolWithoutMainStorageAddedThenItIsIgnored) {
    auto pool = DummyBuffersPool{this->memoryManager.get()};
    pool.mainStorage.reset(nullptr);
    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool));

    EXPECT_EQ(buffersAllocator.bufferPools.size(), 0u);
    EXPECT_EQ(buffersAllocator.getPoolsCount(), 0u);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenNullptrTriedToBeFreedThenItIsNotConsideredValidBuffer) {
    auto pool = DummyBuffersPool{this->memoryManager.get()};
    pool.mainStorage.reset(new DummyBuffer(testVal));
    DummyBuffersAllocator buffersAllocator{pool.params};
    buffersAllocator.addNewBufferPool(std::move(pool));

    EXPECT_TRUE(buffersAllocator.isSizeWithinThreshold(pool.params.smallBufferThreshold));
    EXPECT_FALSE(buffersAllocator.isSizeWithinThreshold(pool.params.smallBufferThreshold + 1));

    auto &chunksToFree = buffersAllocator.bufferPools[0].chunksToFree;
    EXPECT_EQ(chunksToFree.size(), 0u);
    pool.tryFreeFromPoolBuffer(nullptr, 0x42, 42);
    EXPECT_EQ(chunksToFree.size(), 0u);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenNonMainStorageTriedToBeFreedThenItIsNotRegisteredForFreeing) {
    auto pool = DummyBuffersPool{this->memoryManager.get()};
    pool.mainStorage.reset(new DummyBuffer(testVal));
    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool));
    auto otherBuffer = std::make_unique<DummyBuffer>(888);

    auto &chunksToFree = buffersAllocator.bufferPools[0].chunksToFree;
    EXPECT_EQ(chunksToFree.size(), 0u);
    buffersAllocator.tryFreeFromPoolBuffer(otherBuffer.get(), 0x88, 0x400);
    EXPECT_EQ(chunksToFree.size(), 0u);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWithMultiplePoolsWhenSearchingForContributingBufferThenItIsFound) {
    auto pool1 = DummyBuffersPool{this->memoryManager.get()};
    auto pool2 = DummyBuffersPool{this->memoryManager.get()};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    pool2.mainStorage.reset(new DummyBuffer(testVal + 2));
    auto buffer1 = pool1.mainStorage.get();
    auto buffer2 = pool2.mainStorage.get();
    auto otherBuffer = std::make_unique<DummyBuffer>(888);

    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool1));
    buffersAllocator.addNewBufferPool(std::move(pool2));

    EXPECT_TRUE(buffersAllocator.isPoolBuffer(buffer1));
    EXPECT_TRUE(buffersAllocator.isPoolBuffer(buffer2));
    EXPECT_FALSE(buffersAllocator.isPoolBuffer(otherBuffer.get()));
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenChunkOfMainStorageTriedToBeFreedThenItIsEnlistedToBeFreed) {
    auto pool1 = DummyBuffersPool{this->memoryManager.get()};
    auto pool2 = DummyBuffersPool{this->memoryManager.get()};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    pool2.mainStorage.reset(new DummyBuffer(testVal + 2));
    auto poolStorage2 = pool2.mainStorage.get();

    auto buffersAllocator = DummyBuffersAllocator{};
    EXPECT_EQ(0u, buffersAllocator.getPoolsCount());
    buffersAllocator.addNewBufferPool(std::move(pool1));
    EXPECT_EQ(1u, buffersAllocator.getPoolsCount());
    buffersAllocator.addNewBufferPool(std::move(pool2));
    EXPECT_EQ(2u, buffersAllocator.getPoolsCount());

    auto &chunksToFree1 = buffersAllocator.bufferPools[0].chunksToFree;
    auto &chunksToFree2 = buffersAllocator.bufferPools[1].chunksToFree;
    EXPECT_EQ(chunksToFree1.size(), 0u);
    EXPECT_EQ(chunksToFree2.size(), 0u);
    auto chunkSize = DummyBuffersPool::defaultParams.chunkAlignment * 4;
    auto chunkOffset = DummyBuffersPool::defaultParams.chunkAlignment;
    buffersAllocator.tryFreeFromPoolBuffer(poolStorage2, chunkOffset, chunkSize);
    EXPECT_EQ(chunksToFree1.size(), 0u);
    EXPECT_EQ(chunksToFree2.size(), 1u);
    auto [effectiveChunkOffset, size] = chunksToFree2[0];
    EXPECT_EQ(effectiveChunkOffset, chunkOffset);
    EXPECT_EQ(size, chunkSize);

    buffersAllocator.releasePools();
    EXPECT_EQ(buffersAllocator.bufferPools.size(), 0u);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenDrainingPoolsThenOnlyAPoolWithoutAllocationsInUseIsDrained) {
    auto otherMemoryManager = std::make_unique<MockMemoryManager>(this->executionEnvironment);

    auto pool1 = DummyBuffersPool{this->memoryManager.get()};
    auto pool2 = DummyBuffersPool{otherMemoryManager.get()};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    pool2.mainStorage.reset(new DummyBuffer(testVal + 2));
    auto buffer1 = pool1.mainStorage.get();
    auto buffer2 = pool2.mainStorage.get();
    pool1.chunkAllocator.reset(new NEO::HeapAllocator{DummyBuffersPool::defaultParams.startingOffset,
                                                      DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize,
                                                      DummyBuffersPool::defaultParams.chunkAlignment,
                                                      DummyBuffersPool::defaultParams.smallBufferThreshold});
    pool2.chunkAllocator.reset(new NEO::HeapAllocator{DummyBuffersPool::defaultParams.startingOffset,
                                                      DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize,
                                                      DummyBuffersPool::defaultParams.chunkAlignment,
                                                      DummyBuffersPool::defaultParams.smallBufferThreshold});

    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool1));
    buffersAllocator.addNewBufferPool(std::move(pool2));

    auto chunkSize = DummyBuffersPool::defaultParams.chunkAlignment * 4;
    auto chunkOffset = DummyBuffersPool::defaultParams.chunkAlignment;
    for (size_t i = 0; i < 3; i++) {
        auto exampleOffset = chunkOffset + i * chunkSize * 2;
        buffersAllocator.tryFreeFromPoolBuffer(buffer1, exampleOffset, chunkSize);
        buffersAllocator.tryFreeFromPoolBuffer(buffer2, exampleOffset, chunkSize);
    }

    auto &chunksToFree1 = buffersAllocator.bufferPools[0].chunksToFree;
    auto &chunksToFree2 = buffersAllocator.bufferPools[1].chunksToFree;
    auto &freedChunks1 = buffersAllocator.bufferPools[0].freedChunks;
    auto &freedChunks2 = buffersAllocator.bufferPools[1].freedChunks;
    EXPECT_EQ(chunksToFree1.size(), 3u);
    EXPECT_EQ(chunksToFree2.size(), 3u);
    EXPECT_EQ(freedChunks1.size(), 0u);
    EXPECT_EQ(freedChunks2.size(), 0u);

    otherMemoryManager->deferAllocInUse = true;
    buffersAllocator.drainUnderLock();
    EXPECT_EQ(chunksToFree1.size(), 0u);
    EXPECT_EQ(chunksToFree2.size(), 3u);
    ASSERT_EQ(freedChunks1.size(), 3u);
    EXPECT_EQ(freedChunks2.size(), 0u);
    EXPECT_TRUE(buffersAllocator.bufferPools[0].onChunkFreeCalled);
    EXPECT_FALSE(buffersAllocator.bufferPools[1].onChunkFreeCalled);
    for (size_t i = 0; i < 3; i++) {
        auto expectedOffset = chunkOffset + i * chunkSize * 2;
        auto [freedOffset, freedSize] = freedChunks1[i];
        EXPECT_EQ(expectedOffset, freedOffset);
        EXPECT_EQ(chunkSize, freedSize);
    }
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenDrainingPoolsThenOnChunkFreeIgnoredIfNotDefined) {
    auto pool1 = DummyBuffersPool{this->memoryManager.get(), 0x0, nullptr};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    auto buffer1 = pool1.mainStorage.get();
    pool1.chunkAllocator.reset(new NEO::HeapAllocator{DummyBuffersPool::defaultParams.startingOffset,
                                                      DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize,
                                                      DummyBuffersPool::defaultParams.chunkAlignment,
                                                      DummyBuffersPool::defaultParams.smallBufferThreshold});
    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool1));

    auto chunkSize = DummyBuffersPool::defaultParams.chunkAlignment * 4;
    auto chunkOffset = DummyBuffersPool::defaultParams.chunkAlignment;
    for (size_t i = 0; i < 3; i++) {
        auto exampleOffset = chunkOffset + i * chunkSize * 2;
        buffersAllocator.tryFreeFromPoolBuffer(buffer1, exampleOffset, chunkSize);
    }

    auto &chunksToFree1 = buffersAllocator.bufferPools[0].chunksToFree;
    auto &freedChunks1 = buffersAllocator.bufferPools[0].freedChunks;
    EXPECT_EQ(chunksToFree1.size(), 3u);
    EXPECT_EQ(freedChunks1.size(), 0u);

    buffersAllocator.drainUnderLock();
    EXPECT_EQ(chunksToFree1.size(), 0u);
    EXPECT_EQ(freedChunks1.size(), 0u);
    EXPECT_FALSE(buffersAllocator.bufferPools[0].onChunkFreeCalled);
}

TEST_F(AbstractSmallBuffersTest, givenBuffersAllocatorWhenDrainingPoolThenOffsetsPassedToChunkAllocatorAreShiftedProperly) {

    struct ProxyHeapAllocator : public NEO::HeapAllocator {
        using BaseType = NEO::HeapAllocator;

        ProxyHeapAllocator(uint64_t address, uint64_t size, size_t allocationAlignment, size_t threshold)
            : BaseType{address, size, allocationAlignment, threshold} {}

        ~ProxyHeapAllocator() override {
            this->registeredOffsets.clear();
        }

        void free(uint64_t offset, size_t size) override {
            this->registeredOffsets.push_back(offset);
            this->BaseType::free(offset, size);
        }

        std::vector<uint64_t> registeredOffsets;
    };

    auto pool1 = DummyBuffersPool{this->memoryManager.get(), 0x0, nullptr};
    pool1.mainStorage.reset(new DummyBuffer(testVal));
    auto buffer1 = pool1.mainStorage.get();
    pool1.chunkAllocator.reset(new ProxyHeapAllocator{DummyBuffersPool::defaultParams.startingOffset,
                                                      DummyBuffersPool::defaultParams.aggregatedSmallBuffersPoolSize,
                                                      DummyBuffersPool::defaultParams.chunkAlignment,
                                                      DummyBuffersPool::defaultParams.smallBufferThreshold});
    auto buffersAllocator = DummyBuffersAllocator{};
    buffersAllocator.addNewBufferPool(std::move(pool1));

    auto chunkSize = DummyBuffersPool::defaultParams.chunkAlignment * 4;
    auto exampleOffsets = std::array<size_t, 3>{0u, 0u, 0u};
    for (size_t i = 0; i < 3; i++) {
        exampleOffsets[i] = DummyBuffersPool::defaultParams.startingOffset + i * chunkSize * 2;
        buffersAllocator.tryFreeFromPoolBuffer(buffer1, exampleOffsets[i], chunkSize);
    }

    auto &chunksToFree1 = buffersAllocator.bufferPools[0].chunksToFree;
    EXPECT_EQ(chunksToFree1.size(), 3u);

    buffersAllocator.drainUnderLock();
    EXPECT_EQ(chunksToFree1.size(), 0u);
    auto heapAllocator = static_cast<ProxyHeapAllocator *>(buffersAllocator.bufferPools[0].chunkAllocator.get());
    ASSERT_EQ(heapAllocator->registeredOffsets.size(), 3u);
    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(heapAllocator->registeredOffsets[i], exampleOffsets[i] + DummyBuffersPool::defaultParams.startingOffset);
    }
}

struct SmallBuffersParamsTest : public ::testing::Test {
    bool compareSmallBuffersParams(const NEO::SmallBuffersParams &first, const NEO::SmallBuffersParams &second) {
        return first.aggregatedSmallBuffersPoolSize == second.aggregatedSmallBuffersPoolSize &&
               first.smallBufferThreshold == second.smallBufferThreshold &&
               first.chunkAlignment == second.chunkAlignment &&
               first.startingOffset == second.startingOffset;
    }
};

TEST_F(SmallBuffersParamsTest, WhenGettingDefaultParamsThenReturnCorrectValues) {
    auto defaultParams = NEO::SmallBuffersParams::getDefaultParams();

    EXPECT_EQ(2 * MemoryConstants::megaByte, defaultParams.aggregatedSmallBuffersPoolSize);
    EXPECT_EQ(1 * MemoryConstants::megaByte, defaultParams.smallBufferThreshold);
    EXPECT_EQ(MemoryConstants::pageSize64k, defaultParams.chunkAlignment);
    EXPECT_EQ(MemoryConstants::pageSize64k, defaultParams.startingOffset);
}

TEST_F(SmallBuffersParamsTest, WhenGettingLargePagesParamsThenReturnCorrectValues) {
    auto largePagesParams = NEO::SmallBuffersParams::getLargePagesParams();

    EXPECT_EQ(16 * MemoryConstants::megaByte, largePagesParams.aggregatedSmallBuffersPoolSize);
    EXPECT_EQ(2 * MemoryConstants::megaByte, largePagesParams.smallBufferThreshold);
    EXPECT_EQ(MemoryConstants::pageSize64k, largePagesParams.chunkAlignment);
    EXPECT_EQ(MemoryConstants::pageSize64k, largePagesParams.startingOffset);
}

TEST_F(SmallBuffersParamsTest, GivenProductHelperWhenGettingPreferredBufferPoolParamsThenReturnsCorrectValues) {
    auto mockProductHelper = std::make_unique<NEO::MockProductHelper>();

    {
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;
        auto preferredParams = NEO::SmallBuffersParams::getPreferredBufferPoolParams(*mockProductHelper);
        auto expectedParams = NEO::SmallBuffersParams::getDefaultParams();
        EXPECT_TRUE(compareSmallBuffersParams(expectedParams, preferredParams));
    }
    {
        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
        auto preferredParams = NEO::SmallBuffersParams::getPreferredBufferPoolParams(*mockProductHelper);
        auto expectedParams = NEO::SmallBuffersParams::getLargePagesParams();
        EXPECT_TRUE(compareSmallBuffersParams(expectedParams, preferredParams));
    }
}

TEST_F(SmallBuffersParamsTest, GivenBuffersAllocatorWhenSettingDifferentParamsThenGetParamsReturnsExpectedValues) {
    auto buffersAllocator = DummyBuffersAllocator{};

    const NEO::SmallBuffersParams params1{
        .aggregatedSmallBuffersPoolSize = 16 * MemoryConstants::kiloByte,
        .smallBufferThreshold = 1 * MemoryConstants::kiloByte,
        .chunkAlignment = 1024u,
        .startingOffset = 1024u};

    const NEO::SmallBuffersParams params2{
        .aggregatedSmallBuffersPoolSize = 32 * MemoryConstants::megaByte,
        .smallBufferThreshold = 2 * MemoryConstants::megaByte,
        .chunkAlignment = MemoryConstants::pageSize64k,
        .startingOffset = MemoryConstants::pageSize64k};

    buffersAllocator.setParams(params1);
    EXPECT_TRUE(compareSmallBuffersParams(params1, buffersAllocator.getParams()));

    buffersAllocator.setParams(params2);
    EXPECT_TRUE(compareSmallBuffersParams(params2, buffersAllocator.getParams()));
}