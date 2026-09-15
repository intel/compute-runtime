/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace NEO;

namespace NEO {
namespace SysCalls {
extern DWORD getLastErrorResults[];
extern BOOL getLastErrorConstantResult;
} // namespace SysCalls
} // namespace NEO

TEST(CpuInfo, givenIsCpuFlagPresentCalledThenFalseIsReturned) {
    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("fpu"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("vme"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));
}

namespace {

struct FakeProcessorCaches {
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> entries;
    bool failBufferSizeQuery = false;
    bool failProcessorInformationQuery = false;

    void addCache(BYTE level, DWORD cacheSize) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX entry = {};
        entry.Relationship = RelationCache;
        entry.Size = static_cast<DWORD>(sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX));
        entry.Cache.Level = level;
        entry.Cache.CacheSize = cacheSize;
        entries.push_back(entry);
    }

    void addNumaNode() {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX entry = {};
        entry.Relationship = RelationNumaNode;
        entry.Size = static_cast<DWORD>(sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX));
        entries.push_back(entry);
    }

    DWORD getBufferSize() const {
        return static_cast<DWORD>(entries.size() * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX));
    }
};

FakeProcessorCaches *fakeProcessorCaches = nullptr;

BOOL fakeGetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP relationshipType, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer, PDWORD returnedLength) {
    if (buffer == nullptr) {
        *returnedLength = fakeProcessorCaches->getBufferSize();
        return fakeProcessorCaches->failBufferSizeQuery ? TRUE : FALSE;
    }
    if (fakeProcessorCaches->failProcessorInformationQuery) {
        return FALSE;
    }
    std::memcpy(buffer, fakeProcessorCaches->entries.data(), fakeProcessorCaches->getBufferSize());
    *returnedLength = fakeProcessorCaches->getBufferSize();
    return TRUE;
}

struct LastLevelCacheSizeFixture {
    void setUp() {
        fakeProcessorCaches = &processorCaches;
        getLogicalProcessorInformationExBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsGetLogicalProcessorInformationEx)>>(&SysCalls::sysCallsGetLogicalProcessorInformationEx, fakeGetLogicalProcessorInformationEx);
        lastErrorConstantResultBackup = std::make_unique<VariableBackup<BOOL>>(&SysCalls::getLastErrorConstantResult, TRUE);
        lastErrorBackup = std::make_unique<VariableBackup<DWORD>>(&SysCalls::getLastErrorResults[0], ERROR_INSUFFICIENT_BUFFER);
    }

    void tearDown() {
        fakeProcessorCaches = nullptr;
    }

    size_t detectLastLevelCacheSize() {
        return CpuInfo::getLastLevelCacheSizeFunc();
    }

    FakeProcessorCaches processorCaches;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsGetLogicalProcessorInformationEx)>> getLogicalProcessorInformationExBackup;
    std::unique_ptr<VariableBackup<BOOL>> lastErrorConstantResultBackup;
    std::unique_ptr<VariableBackup<DWORD>> lastErrorBackup;
};

using LastLevelCacheSizeTest = Test<LastLevelCacheSizeFixture>;

} // namespace

TEST_F(LastLevelCacheSizeTest, WhenLastLevelCacheIsReportedThenItsSizeIsReturned) {
    processorCaches.addCache(1u, 48u * MemoryConstants::kiloByte);
    processorCaches.addCache(2u, 2u * MemoryConstants::megaByte);
    processorCaches.addCache(3u, 105u * MemoryConstants::megaByte);

    EXPECT_EQ(105u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenMultipleLastLevelCachesReportedWhenDetectingThenLargestOneIsReturned) {
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);
    processorCaches.addCache(3u, 12u * MemoryConstants::megaByte);

    EXPECT_EQ(12u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenLowerLevelCacheLargerThanLastLevelCacheWhenDetectingThenLastLevelCacheSizeIsReturned) {
    processorCaches.addCache(2u, 8u * MemoryConstants::megaByte);
    processorCaches.addCache(3u, 4u * MemoryConstants::megaByte);

    EXPECT_EQ(4u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenNoProcessorReportsLastLevelCacheWhenDetectingThenLargestSeenCacheIsReturned) {
    processorCaches.addCache(1u, 32u * MemoryConstants::kiloByte);
    processorCaches.addCache(2u, 4u * MemoryConstants::megaByte);

    EXPECT_EQ(4u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenNonCacheRelationshipEntriesWhenDetectingThenTheyAreIgnored) {
    processorCaches.addNumaNode();
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenEntryWithoutSizeWhenDetectingThenScanIsStopped) {
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);
    processorCaches.addCache(3u, 12u * MemoryConstants::megaByte);
    processorCaches.entries[0].Size = 0u;

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenBufferSizeQueryUnexpectedlySucceedingWhenDetectingThenZeroIsReturned) {
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);
    processorCaches.failBufferSizeQuery = true;

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenLastErrorOtherThanInsufficientBufferWhenDetectingThenZeroIsReturned) {
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);
    SysCalls::getLastErrorResults[0] = ERROR_INVALID_PARAMETER;

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenProcessorInformationQueryFailingWhenDetectingThenZeroIsReturned) {
    processorCaches.addCache(3u, 8u * MemoryConstants::megaByte);
    processorCaches.failProcessorInformationQuery = true;

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}
