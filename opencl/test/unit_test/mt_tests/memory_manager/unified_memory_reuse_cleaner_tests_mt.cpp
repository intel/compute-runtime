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
    MockUnifiedMemoryReuseCleaner cleaner(false);
    cleaner.callBaseStartThread = true;
    cleaner.callBaseTrimOldInCaches = false;
    EXPECT_EQ(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    cleaner.startThread();
    EXPECT_NE(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    EXPECT_FALSE(cleaner.runCleaning.load());
    EXPECT_TRUE(cleaner.keepCleaning.load());

    EXPECT_FALSE(cleaner.trimOldInCachesCalled);
    auto svmAllocCache = std::make_unique<SVMAllocsManager::SvmAllocationCache>();
    MockMemoryManager mockMemoryManager;
    constexpr size_t svmAllocSize = 1024;
    mockMemoryManager.usmReuseInfo.init(svmAllocSize, svmAllocSize);
    svmAllocCache->memoryManager = &mockMemoryManager;
    cleaner.registerSvmAllocationCache(svmAllocCache.get());
    EXPECT_TRUE(cleaner.runCleaning.load());

    // Caches are empty, ensure cleaner thread is sleeping on condition var
    while (!cleaner.sleepOnEmptyCachesCondVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(cleaner.sleepOnEmptyCachesCondVar.load());
    EXPECT_FALSE(cleaner.trimOldInCachesCalled);

    // Wake cleaner thread to proceed some data
    SvmAllocationData allocData{0};
    svmAllocCache->insert(svmAllocSize, nullptr, &allocData, false);
    EXPECT_TRUE(cleaner.sleepOnEmptyCachesCondVar.load());
    while (cleaner.sleepOnEmptyCachesCondVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_FALSE(cleaner.sleepOnEmptyCachesCondVar.load());
    EXPECT_TRUE(cleaner.trimOldInCachesCalled);

    // Empty caches to sleep on condition var again
    cleaner.clearCaches();
    while (!cleaner.sleepOnEmptyCachesCondVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(cleaner.sleepOnEmptyCachesCondVar.load());

    cleaner.stopThread();
    EXPECT_EQ(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    EXPECT_FALSE(cleaner.runCleaning.load());
    EXPECT_FALSE(cleaner.keepCleaning.load());
}

TEST(UnifiedMemoryReuseCleanerTestsMt, givenUnifiedMemoryReuseCleanerWithNotStartedCleaningWhenShuttingDownThenNoHang) {
    MockUnifiedMemoryReuseCleaner cleaner(false);
    cleaner.callBaseStartThread = true;
    cleaner.callBaseTrimOldInCaches = false;
    cleaner.startThread();
    EXPECT_NE(nullptr, cleaner.unifiedMemoryReuseCleanerThread);

    std::this_thread::yield();
    cleaner.stopThread();
}

} // namespace NEO
