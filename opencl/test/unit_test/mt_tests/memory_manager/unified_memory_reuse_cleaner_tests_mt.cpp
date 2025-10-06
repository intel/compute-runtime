/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_usm_memory_reuse_cleaner.h"
#include "shared/test/common/test_macros/test.h"
namespace NEO {

TEST(UnifiedMemoryReuseCleanerTestsMt, givenUnifiedMemoryReuseCleanerWhenSleepExpiredThenTrimOldInCachesIsCalled) {
    MockUnifiedMemoryReuseCleaner cleaner(false);
    cleaner.callBaseStartThread = true;
    cleaner.callBaseTrimOldInCaches = false;
    EXPECT_EQ(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    cleaner.startThread();
    EXPECT_NE(nullptr, cleaner.unifiedMemoryReuseCleanerThread);
    EXPECT_FALSE(cleaner.runCleaning.load());
    EXPECT_TRUE(cleaner.keepCleaning.load());

    EXPECT_FALSE(cleaner.trimOldInCachesCalled);
    cleaner.registerSvmAllocationCache(nullptr);
    EXPECT_TRUE(cleaner.runCleaning.load());

    while (false == cleaner.trimOldInCachesCalled) {
        std::this_thread::yield();
    }
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