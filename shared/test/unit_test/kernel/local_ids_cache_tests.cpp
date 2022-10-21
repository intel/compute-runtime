/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/kernel/local_ids_cache.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

struct LocalIdsCacheFixture {
    class MockLocalIdsCache : public NEO::LocalIdsCache {
      public:
        using Base = NEO::LocalIdsCache;
        using Base::Base;
        using Base::cache;
        MockLocalIdsCache(size_t cacheSize) : Base(cacheSize, {0, 1, 2}, 32, 32, false){};
    };

    void setUp() {
        localIdsCache = std::make_unique<MockLocalIdsCache>(1);
    }
    void tearDown() {}

    std::array<uint8_t, 2048> perThreadData = {0};
    Vec3<uint16_t> groupSize = {128, 2, 1};
    std::unique_ptr<MockLocalIdsCache> localIdsCache;
};

using LocalIdsCacheTest = Test<LocalIdsCacheFixture>;
TEST_F(LocalIdsCacheTest, GivenCacheMissWhenGetLocalIdsForGroupThenNewEntryIsCommitedIntoLeastUsedEntry) {
    localIdsCache->cache.resize(2);
    localIdsCache->cache[0].accessCounter = 2U;
    localIdsCache->setLocalIdsForGroup(groupSize, perThreadData.data());

    EXPECT_EQ(groupSize, localIdsCache->cache[1].groupSize);
    EXPECT_NE(nullptr, localIdsCache->cache[1].localIdsData);
    EXPECT_EQ(1536U, localIdsCache->cache[1].localIdsSize);
    EXPECT_EQ(1536U, localIdsCache->cache[1].localIdsSizeAllocated);
    EXPECT_EQ(1U, localIdsCache->cache[1].accessCounter);
}

TEST_F(LocalIdsCacheTest, GivenEntryInCacheWhenGetLocalIdsForGroupThenEntryFromCacheIsUsed) {
    localIdsCache->cache[0].groupSize = groupSize;
    localIdsCache->cache[0].localIdsData = static_cast<uint8_t *>(alignedMalloc(512, 32));
    localIdsCache->cache[0].localIdsSize = 512U;
    localIdsCache->cache[0].localIdsSizeAllocated = 512U;
    localIdsCache->cache[0].accessCounter = 1U;
    localIdsCache->setLocalIdsForGroup(groupSize, perThreadData.data());
    EXPECT_EQ(2U, localIdsCache->cache[0].accessCounter);
}

TEST_F(LocalIdsCacheTest, GivenEntryWithBiggerBufferAllocatedWhenGetLocalIdsForGroupThenBufferIsReused) {
    localIdsCache->cache[0].groupSize = {4, 1, 1};
    localIdsCache->cache[0].localIdsData = static_cast<uint8_t *>(alignedMalloc(512, 32));
    localIdsCache->cache[0].localIdsSize = 512U;
    localIdsCache->cache[0].localIdsSizeAllocated = 512U;
    localIdsCache->cache[0].accessCounter = 2U;
    const auto localIdsData = localIdsCache->cache[0].localIdsData;

    groupSize = {2, 1, 1};
    localIdsCache->setLocalIdsForGroup(groupSize, perThreadData.data());
    EXPECT_EQ(1U, localIdsCache->cache[0].accessCounter);
    EXPECT_EQ(192U, localIdsCache->cache[0].localIdsSize);
    EXPECT_EQ(512U, localIdsCache->cache[0].localIdsSizeAllocated);
    EXPECT_EQ(localIdsData, localIdsCache->cache[0].localIdsData);
}

TEST_F(LocalIdsCacheTest, GivenValidLocalIdsCacheWhenGettingLocalIdsSizePerThreadThenCorrectValueIsReturned) {
    auto localIdsSizePerThread = localIdsCache->getLocalIdsSizePerThread();
    EXPECT_EQ(192U, localIdsSizePerThread);
}

TEST_F(LocalIdsCacheTest, GivenValidLocalIdsCacheWhenGettingLocalIdsSizeForGroupThenCorrectValueIsReturned) {
    auto localIdsSizePerThread = localIdsCache->getLocalIdsSizeForGroup(groupSize);
    EXPECT_EQ(1536U, localIdsSizePerThread);
}
