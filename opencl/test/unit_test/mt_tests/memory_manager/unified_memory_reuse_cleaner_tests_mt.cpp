/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_reuse_cleaner.h"
#include "shared/test/common/test_macros/test.h"
namespace NEO {

TEST(UnifiedMemoryReuseCleanerTestsMt, givenUnifiedMemoryReuseCleanerWhenCachesAreEmptyThenWorkerThreadIsWaitingOnConditionVar) {
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.executionEnvironment.unifiedMemoryReuseCleaner.reset(new MockUnifiedMemoryReuseCleaner(false));
    MockUnifiedMemoryReuseCleaner &cleaner = *static_cast<MockUnifiedMemoryReuseCleaner *>(mockMemoryManager.executionEnvironment.unifiedMemoryReuseCleaner.get());

    cleaner.callBaseStartThread = true;
    cleaner.callBaseTrimOldInCaches = false;
    EXPECT_EQ(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    cleaner.startThread();
    {
        auto cleanerThread = cleaner.unifiedMemoryReuseCleanerThread.get();
        EXPECT_NE(nullptr, cleanerThread);

        cleaner.startThread();
        EXPECT_EQ(cleanerThread, cleaner.unifiedMemoryReuseCleanerThread.get());
    }
    EXPECT_TRUE(cleaner.keepCleaning.load());

    // Nothing to do, sleeping on condition var
    while (!cleaner.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(cleaner.waitOnConditionVar.load());
    EXPECT_TRUE(cleaner.isEmpty());
    EXPECT_FALSE(cleaner.trimOldInCachesCalled);

    auto svmAllocCache = std::make_unique<SVMAllocsManager::SvmAllocationCache>();

    constexpr size_t svmAllocSize = 1024;
    mockMemoryManager.usmReuseInfo.init(svmAllocSize, svmAllocSize);
    svmAllocCache->memoryManager = &mockMemoryManager;
    cleaner.registerSvmAllocationCache(svmAllocCache.get());

    // Caches are empty, ensure cleaner thread is still waiting on condition var
    while (!cleaner.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(cleaner.waitOnConditionVar.load());
    EXPECT_TRUE(cleaner.isEmpty());
    EXPECT_FALSE(cleaner.trimOldInCachesCalled);

    // Wake cleaner thread to proceed some data
    cleaner.waitOnConditionVar.store(false);
    EXPECT_FALSE(cleaner.waitOnConditionVar.load());
    SvmAllocationData allocData{0};
    svmAllocCache->insert(svmAllocSize, nullptr, &allocData, false);
    while (!cleaner.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(cleaner.waitOnConditionVar.load());
    EXPECT_TRUE(cleaner.isEmpty());
    EXPECT_TRUE(cleaner.trimOldInCachesCalled);

    cleaner.stopThread();
    EXPECT_EQ(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    EXPECT_FALSE(cleaner.keepCleaning.load());
}

TEST(UnifiedMemoryReuseCleanerTestsMt, givenUnifiedMemoryReuseCleanerWhenShuttingDownThenNoHang) {
    MockUnifiedMemoryReuseCleaner cleaner(false);
    cleaner.callBaseStartThread = true;
    cleaner.callBaseTrimOldInCaches = false;
    cleaner.startThread();
    EXPECT_NE(nullptr, cleaner.unifiedMemoryReuseCleanerThread);

    std::this_thread::yield();
    cleaner.stopThread();
}

} // namespace NEO
